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
