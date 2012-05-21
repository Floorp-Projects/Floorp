# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''expandlibs_deps.py takes a list of key/value pairs and prints, for each
key, a list of all dependencies corresponding to the corresponding value.
Libraries are thus expanded, and their descriptor is also made part of this
list.
The list of key/value is passed on the command line in the form
"var1 = value , var2 = value ..."
'''
import sys
import os
from expandlibs import ExpandArgs, relativize
import expandlibs_config as conf

class ExpandLibsDeps(ExpandArgs):
    def _expand_desc(self, arg):
        objs = super(ExpandLibsDeps, self)._expand_desc(arg)
        if os.path.exists(arg + conf.LIBS_DESC_SUFFIX):
            objs += [relativize(arg + conf.LIBS_DESC_SUFFIX)]
        return objs

def split_args(args):
    '''Transforms a list in the form
    ['var1', '=', 'value', ',', 'var2', '=', 'other', 'value', ',' ...]
    into a corresponding dict 
    { 'var1': ['value'],
      'var2': ['other', 'value'], ... }'''

    result = {}
    while args:
        try:
            i = args.index(',')
            l = args[:i]
            args[:i + 1] = []
        except:
            l = args
            args = []
        if l[1] != '=':
            raise RuntimeError('Expected "var = value" format')
        result[l[0]] = l[2:]
    return result

if __name__ == '__main__':
    for key, value in split_args(sys.argv[1:]).iteritems():
        expanded = ExpandLibsDeps(value)
        if os.sep == '\\':
            expanded = [o.replace(os.sep, '/') for o in expanded]
        print "%s = %s" % (key, " ".join(expanded))
