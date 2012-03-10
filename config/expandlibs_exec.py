# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is a build helper for libraries
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Mike Hommey <mh@glandium.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

'''expandlibs-exec.py applies expandlibs rules, and some more (see below) to
a given command line, and executes that command line with the expanded
arguments.

With the --extract argument (useful for e.g. $(AR)), it extracts object files
from static libraries (or use those listed in library descriptors directly).

With the --uselist argument (useful for e.g. $(CC)), it replaces all object
files with a list file. This can be used to avoid limitations in the length
of a command line. The kind of list file format used depends on the
EXPAND_LIBS_LIST_STYLE variable: 'list' for MSVC style lists (@file.list)
or 'linkerscript' for GNU ld linker scripts.
See https://bugzilla.mozilla.org/show_bug.cgi?id=584474#c59 for more details.

With the --symbol-order argument, followed by a file name, it will add the
relevant linker options to change the order in which the linker puts the
symbols appear in the resulting binary. Only works for ELF targets.
'''
from __future__ import with_statement
import sys
import os
from expandlibs import ExpandArgs, relativize, isObject
import expandlibs_config as conf
from optparse import OptionParser
import subprocess
import tempfile
import shutil
import subprocess
import re

# The are the insert points for a GNU ld linker script, assuming a more
# or less "standard" default linker script. This is not a dict because
# order is important.
SECTION_INSERT_BEFORE = [
  ('.text', '.fini'),
  ('.rodata', '.rodata1'),
  ('.data.rel.ro', '.dynamic'),
  ('.data', '.data1'),
]

class ExpandArgsMore(ExpandArgs):
    ''' Meant to be used as 'with ExpandArgsMore(args) as ...: '''
    def __enter__(self):
        self.tmp = []
        return self
        
    def __exit__(self, type, value, tb):
        '''Automatically remove temporary files'''
        for tmp in self.tmp:
            if os.path.isdir(tmp):
                shutil.rmtree(tmp, True)
            else:
                os.remove(tmp)

    def extract(self):
        self[0:] = self._extract(self)

    def _extract(self, args):
        '''When a static library name is found, either extract its contents
        in a temporary directory or use the information found in the
        corresponding lib descriptor.
        '''
        ar_extract = conf.AR_EXTRACT.split()
        newlist = []
        for arg in args:
            if os.path.splitext(arg)[1] == conf.LIB_SUFFIX:
                if os.path.exists(arg + conf.LIBS_DESC_SUFFIX):
                    newlist += self._extract(self._expand_desc(arg))
                elif os.path.exists(arg) and len(ar_extract):
                    tmp = tempfile.mkdtemp(dir=os.curdir)
                    self.tmp.append(tmp)
                    subprocess.call(ar_extract + [os.path.abspath(arg)], cwd=tmp)
                    objs = []
                    for root, dirs, files in os.walk(tmp):
                        objs += [relativize(os.path.join(root, f)) for f in files if isObject(f)]
                    newlist += objs
                else:
                    newlist += [arg]
            else:
                newlist += [arg]
        return newlist

    def makelist(self):
        '''Replaces object file names with a temporary list file, using a
        list format depending on the EXPAND_LIBS_LIST_STYLE variable
        '''
        objs = [o for o in self if isObject(o)]
        if not len(objs): return
        fd, tmp = tempfile.mkstemp(suffix=".list",dir=os.curdir)
        if conf.EXPAND_LIBS_LIST_STYLE == "linkerscript":
            content = ["INPUT(%s)\n" % obj for obj in objs]
            ref = tmp
        elif conf.EXPAND_LIBS_LIST_STYLE == "list":
            content = ["%s\n" % obj for obj in objs]
            ref = "@" + tmp
        else:
            os.close(fd)
            os.remove(tmp)
            return
        self.tmp.append(tmp)
        f = os.fdopen(fd, "w")
        f.writelines(content)
        f.close()
        idx = self.index(objs[0])
        newlist = self[0:idx] + [ref] + [item for item in self[idx:] if item not in objs]
        self[0:] = newlist

    def _getFoldedSections(self):
        '''Returns a dict about folded sections.
        When section A and B are folded into section C, the dict contains:
        { 'A': 'C',
          'B': 'C',
          'C': ['A', 'B'] }'''
        if not conf.LD_PRINT_ICF_SECTIONS:
            return {}

        proc = subprocess.Popen(self + [conf.LD_PRINT_ICF_SECTIONS], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        (stdout, stderr) = proc.communicate()
        result = {}
        # gold's --print-icf-sections output looks like the following:
        # ld: ICF folding section '.section' in file 'file.o'into '.section' in file 'file.o'
        # In terms of words, chances are this will change in the future,
        # especially considering "into" is misplaced. Splitting on quotes
        # seems safer.
        for l in stderr.split('\n'):
            quoted = l.split("'")
            if len(quoted) > 5 and quoted[1] != quoted[5]:
                result[quoted[1]] = quoted[5]
                if quoted[5] in result:
                    result[quoted[5]].append(quoted[1])
                else:
                    result[quoted[5]] = [quoted[1]]
        return result

    def _getOrderedSections(self, ordered_symbols):
        '''Given an ordered list of symbols, returns the corresponding list
        of sections following the order.'''
        if not conf.EXPAND_LIBS_ORDER_STYLE in ['linkerscript', 'section-ordering-file']:
            raise Exception('EXPAND_LIBS_ORDER_STYLE "%s" is not supported' % conf.EXPAND_LIBS_ORDER_STYLE)
        finder = SectionFinder([arg for arg in self if isObject(arg) or os.path.splitext(arg)[1] == conf.LIB_SUFFIX])
        folded = self._getFoldedSections()
        sections = set()
        ordered_sections = []
        for symbol in ordered_symbols:
            symbol_sections = finder.getSections(symbol)
            all_symbol_sections = []
            for section in symbol_sections:
                if section in folded:
                    if isinstance(folded[section], str):
                        section = folded[section]
                    all_symbol_sections.append(section)
                    all_symbol_sections.extend(folded[section])
                else:
                    all_symbol_sections.append(section)
            for section in all_symbol_sections:
                if not section in sections:
                    ordered_sections.append(section)
                    sections.add(section)
        return ordered_sections

    def orderSymbols(self, order):
        '''Given a file containing a list of symbols, adds the appropriate
        argument to make the linker put the symbols in that order.'''
        with open(order) as file:
            sections = self._getOrderedSections([l.strip() for l in file.readlines() if l.strip()])
        split_sections = {}
        linked_sections = [s[0] for s in SECTION_INSERT_BEFORE]
        for s in sections:
            for linked_section in linked_sections:
                if s.startswith(linked_section):
                    if linked_section in split_sections:
                        split_sections[linked_section].append(s)
                    else:
                        split_sections[linked_section] = [s]
                    break
        content = []
        # Order is important
        linked_sections = [s for s in linked_sections if s in split_sections]

        if conf.EXPAND_LIBS_ORDER_STYLE == 'section-ordering-file':
            option = '-Wl,--section-ordering-file,%s'
            content = sections
            for linked_section in linked_sections:
                content.extend(split_sections[linked_section])
                content.append('%s.*' % linked_section)
                content.append(linked_section)

        elif conf.EXPAND_LIBS_ORDER_STYLE == 'linkerscript':
            option = '-Wl,-T,%s'
            section_insert_before = dict(SECTION_INSERT_BEFORE)
            for linked_section in linked_sections:
                content.append('SECTIONS {')
                content.append('  %s : {' % linked_section)
                content.extend('    *(%s)' % s for s in split_sections[linked_section])
                content.append('  }')
                content.append('}')
                content.append('INSERT BEFORE %s' % section_insert_before[linked_section])
        else:
            raise Exception('EXPAND_LIBS_ORDER_STYLE "%s" is not supported' % conf.EXPAND_LIBS_ORDER_STYLE)

        fd, tmp = tempfile.mkstemp(dir=os.curdir)
        f = os.fdopen(fd, "w")
        f.write('\n'.join(content)+'\n')
        f.close()
        self.tmp.append(tmp)
        self.append(option % tmp)

class SectionFinder(object):
    '''Instances of this class allow to map symbol names to sections in
    object files.'''

    def __init__(self, objs):
        '''Creates an instance, given a list of object files.'''
        if not conf.EXPAND_LIBS_ORDER_STYLE in ['linkerscript', 'section-ordering-file']:
            raise Exception('EXPAND_LIBS_ORDER_STYLE "%s" is not supported' % conf.EXPAND_LIBS_ORDER_STYLE)
        self.mapping = {}
        for obj in objs:
            if not isObject(obj) and os.path.splitext(obj)[1] != conf.LIB_SUFFIX:
                raise Exception('%s is not an object nor a static library' % obj)
            for symbol, section in SectionFinder._getSymbols(obj):
                sym = SectionFinder._normalize(symbol)
                if sym in self.mapping:
                    if not section in self.mapping[sym]:
                        self.mapping[sym].append(section)
                else:
                    self.mapping[sym] = [section]

    def getSections(self, symbol):
        '''Given a symbol, returns a list of sections containing it or the
        corresponding thunks. When the given symbol is a thunk, returns the
        list of sections containing its corresponding normal symbol and the
        other thunks for that symbol.'''
        sym = SectionFinder._normalize(symbol)
        if sym in self.mapping:
            return self.mapping[sym]
        return []

    @staticmethod
    def _normalize(symbol):
        '''For normal symbols, return the given symbol. For thunks, return
        the corresponding normal symbol.'''
        if re.match('^_ZThn[0-9]+_', symbol):
            return re.sub('^_ZThn[0-9]+_', '_Z', symbol)
        return symbol

    @staticmethod
    def _getSymbols(obj):
        '''Returns a list of (symbol, section) contained in the given object
        file.'''
        proc = subprocess.Popen(['objdump', '-t', obj], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        (stdout, stderr) = proc.communicate()
        syms = []
        for line in stdout.splitlines():
            # Each line has the following format:
            # <addr> [lgu!][w ][C ][W ][Ii ][dD ][FfO ] <section>\t<length> <symbol>
            tmp = line.split(' ',1)
            # This gives us ["<addr>", "[lgu!][w ][C ][W ][Ii ][dD ][FfO ] <section>\t<length> <symbol>"]
            # We only need to consider cases where "<section>\t<length> <symbol>" is present,
            # and where the [FfO] flag is either F (function) or O (object).
            if len(tmp) > 1 and len(tmp[1]) > 6 and tmp[1][6] in ['O', 'F']:
                tmp = tmp[1][8:].split()
                # That gives us ["<section>","<length>", "<symbol>"]
                syms.append((tmp[-1], tmp[0]))
        return syms

def main():
    parser = OptionParser()
    parser.add_option("--extract", action="store_true", dest="extract",
        help="when a library has no descriptor file, extract it first, when possible")
    parser.add_option("--uselist", action="store_true", dest="uselist",
        help="use a list file for objects when executing a command")
    parser.add_option("--verbose", action="store_true", dest="verbose",
        help="display executed command and temporary files content")
    parser.add_option("--symbol-order", dest="symbol_order", metavar="FILE",
        help="use the given list of symbols to order symbols in the resulting binary when using with a linker")

    (options, args) = parser.parse_args()

    with ExpandArgsMore(args) as args:
        if options.extract:
            args.extract()
        if options.symbol_order:
            args.orderSymbols(options.symbol_order)
        if options.uselist:
            args.makelist()

        if options.verbose:
            print >>sys.stderr, "Executing: " + " ".join(args)
            for tmp in [f for f in args.tmp if os.path.isfile(f)]:
                print >>sys.stderr, tmp + ":"
                with open(tmp) as file:
                    print >>sys.stderr, "".join(["    " + l for l in file.readlines()])
            sys.stderr.flush()
        exit(subprocess.call(args))

if __name__ == '__main__':
    main()
