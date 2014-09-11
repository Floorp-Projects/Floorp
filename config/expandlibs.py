# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
import sys, os, errno
import expandlibs_config as conf

def ensureParentDir(file):
    '''Ensures the directory parent to the given file exists'''
    dir = os.path.dirname(file)
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError, error:
            if error.errno != errno.EEXIST:
                raise

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

def isDynamicLib(path):
    '''Returns whether the given path points to a dynamic library, that is,
    ends with DLL_SUFFIX.'''
    # On mac, the xul library is named XUL, instead of libxul.dylib. Assume any
    # file by that name is a dynamic library.
    return os.path.splitext(path)[1] == conf.DLL_SUFFIX or os.path.basename(path) == 'XUL'

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
        self._descs = set()
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
        desc = os.path.abspath(arg + conf.LIBS_DESC_SUFFIX)
        if os.path.exists(desc):
            if desc in self._descs:
                return []
            self._descs.add(desc)
            with open(desc, 'r') as f:
                desc = LibDescriptor(f.readlines())
            objs = [relativize(o) for o in desc['OBJS']]
            for lib in desc['LIBS']:
                objs += self._expand(lib)
            return objs
        return [arg]

if __name__ == '__main__':
    print " ".join(ExpandArgs(sys.argv[1:]))
