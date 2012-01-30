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

'''Expandlibs is a system that allows to replace some libraries with a
descriptor file containing some linking information about them.

The descriptor file format is as follows:
---8<-----
OBJS = a.o b.o ...
LIBS = libfoo.a libbar.a ...
--->8-----

(In the example above, OBJ_SUFFIX is o and LIB_SUFFIX is a).

Expandlibs also canonicalizes how to pass libraries to the linker, such
that only the ${LIB_PREFIX}${ROOT}.${LIB_SUFFIX} form needs to be used:
given a list of files, expandlibs will replace items with the form
${LIB_PREFIX}${ROOT}.${LIB_SUFFIX} following these rules:

- If a ${DLL_PREFIX}${ROOT}.${DLL_SUFFIX} or
  ${DLL_PREFIX}${ROOT}.${IMPORT_LIB_SUFFIX} file exists, use that instead
- If the ${LIB_PREFIX}${ROOT}.${LIB_SUFFIX} file exists, use it
- If a ${LIB_PREFIX}${ROOT}.${LIB_SUFFIX}.${LIB_DESC_SUFFIX} file exists,
  replace ${LIB_PREFIX}${ROOT}.${LIB_SUFFIX} with the OBJS and LIBS the
  descriptor contains. And for each of these LIBS, also apply the same
  rules.
'''
from __future__ import with_statement
import sys, os
import expandlibs_config as conf

def relativize(path):
    '''Returns a path relative to the current working directory, if it is
    shorter than the given path'''
    def splitpath(path):
        dir, file = os.path.split(path)
        if os.path.splitdrive(dir)[1] == os.sep:
            return [file]
        return splitpath(dir) + [file]

    if not os.path.exists(path):
        return path
    curdir = splitpath(os.path.abspath(os.curdir))
    abspath = splitpath(os.path.abspath(path))
    while curdir and abspath and curdir[0] == abspath[0]:
        del curdir[0]
        del abspath[0]
    if not curdir and not abspath:
        return '.'
    relpath = os.path.join(*[os.pardir for i in curdir] + abspath)
    if len(path) > len(relpath):
        return relpath
    return path

def isObject(path):
    '''Returns whether the given path points to an object file, that is,
    ends with OBJ_SUFFIX or .i_o'''
    return os.path.splitext(path)[1] in [conf.OBJ_SUFFIX, '.i_o']

class LibDescriptor(dict):
    KEYS = ['OBJS', 'LIBS']

    def __init__(self, content=None):
        '''Creates an instance of a lib descriptor, initialized with contents
        from a list of strings when given. This is intended for use with
        file.readlines()'''
        if isinstance(content, list) and all([isinstance(item, str) for item in content]):
            pass
        elif content is not None:
            raise TypeError("LibDescriptor() arg 1 must be None or a list of strings")
        super(LibDescriptor, self).__init__()
        for key in self.KEYS:
            self[key] = []
        if not content:
            return
        for key, value in [(s.strip() for s in item.split('=', 2)) for item in content if item.find('=') >= 0]:
            if key in self.KEYS:
                self[key] = value.split()

    def __str__(self):
        '''Serializes the lib descriptor'''
        return '\n'.join('%s = %s' % (k, ' '.join(self[k])) for k in self.KEYS if len(self[k]))

class ExpandArgs(list):
    def __init__(self, args):
        '''Creates a clone of the |args| list and performs file expansion on
        each item it contains'''
        super(ExpandArgs, self).__init__()
        for arg in args:
            self += self._expand(arg)

    def _expand(self, arg):
        '''Internal function doing the actual work'''
        (root, ext) = os.path.splitext(arg)
        if ext != conf.LIB_SUFFIX or not os.path.basename(root).startswith(conf.LIB_PREFIX):
            return [relativize(arg)]
        if len(conf.IMPORT_LIB_SUFFIX):
            dll = root + conf.IMPORT_LIB_SUFFIX
        else:
            dll = root.replace(conf.LIB_PREFIX, conf.DLL_PREFIX, 1) + conf.DLL_SUFFIX
        if os.path.exists(dll):
            return [relativize(dll)]
        if os.path.exists(arg):
            return [relativize(arg)]
        return self._expand_desc(arg)

    def _expand_desc(self, arg):
        '''Internal function taking care of lib descriptor expansion only'''
        if os.path.exists(arg + conf.LIBS_DESC_SUFFIX):
            with open(arg + conf.LIBS_DESC_SUFFIX, 'r') as f:
                desc = LibDescriptor(f.readlines())
            objs = [relativize(o) for o in desc['OBJS']]
            for lib in desc['LIBS']:
                objs += self._expand(lib)
            return objs
        return [arg]

if __name__ == '__main__':
    print " ".join(ExpandArgs(sys.argv[1:]))
