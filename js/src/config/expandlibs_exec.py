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
'''
from __future__ import with_statement
import sys
import os
from expandlibs import ExpandArgs, relativize
import expandlibs_config as conf
from optparse import OptionParser
import subprocess
import tempfile
import shutil

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
                        objs += [relativize(os.path.join(root, f)) for f in files if os.path.splitext(f)[1] in [conf.OBJ_SUFFIX, '.i_o']]
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
        objs = [o for o in self if os.path.splitext(o)[1] == conf.OBJ_SUFFIX]
        if not len(objs): return
        fd, tmp = tempfile.mkstemp(suffix=".list",dir=os.curdir)
        if conf.EXPAND_LIBS_LIST_STYLE == "linkerscript":
            content = ["INPUT(%s)\n" % obj for obj in objs]
            ref = tmp
        elif conf.EXPAND_LIBS_LIST_STYLE == "list":
            content = ["%s\n" % obj for obj in objs]
            ref = "@" + tmp
        else:
            os.remove(tmp)
            return
        self.tmp.append(tmp)
        f = os.fdopen(fd, "w")
        f.writelines(content)
        f.close()
        idx = self.index(objs[0])
        newlist = self[0:idx] + [ref] + [item for item in self[idx:] if item not in objs]
        self[0:] = newlist

def main():
    parser = OptionParser()
    parser.add_option("--extract", action="store_true", dest="extract",
        help="when a library has no descriptor file, extract it first, when possible")
    parser.add_option("--uselist", action="store_true", dest="uselist",
        help="use a list file for objects when executing a command")
    parser.add_option("--verbose", action="store_true", dest="verbose",
        help="display executed command and temporary files content")

    (options, args) = parser.parse_args()

    with ExpandArgsMore(args) as args:
        if options.extract:
            args.extract()
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
