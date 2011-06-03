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
# The Original Code is Mozilla Firefox.
#
# The Initial Developer of the Original Code is
# The Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
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

import string, sys

def usage():
    print >>sys.stderr, """
%s template_file -t unit_tests... -e extra_protocols...

  TEMPLATE_FILE is used to generate to generate the unit-tester .cpp
  UNIT_TESTS are the top-level protocols defining unit tests
  EXTRA_PROTOCOLS are top-level protocols for subprocesses that can be
                  spawned in tests but are not unit tests in and of
                  themselves
"""% (sys.argv[0])
    sys.exit(1)

def main(argv):
    template = argv[1]

    if argv[2] != '-t': usage()
    i = 3
    unittests = []
    while argv[i] != '-e':
        unittests.append(argv[i])
        i += 1

    extras = argv[(i+1):]

    includes = '\n'.join([
        '#include "%s.h"'% (t) for t in unittests ])


    enum_values = '\n'.join([
        '    %s,'% (t) for t in unittests+extras ])
    last_enum = unittests[-1]


    string_to_enums = '\n'.join([
        '''    else if (!strcmp(aString, "%s"))
        return %s;'''% (t, t) for t in unittests+extras ])

    enum_to_strings = '\n'.join([
        '''    case %s:
        return "%s";'''%(t, t) for t in unittests+extras ])

    parent_delete_cases = '\n'.join([
'''    case %s: {
           delete reinterpret_cast<%sParent*>(gParentActor);
           return;
       }
'''% (t, t) for t in unittests ])

    parent_main_cases = '\n'.join([
'''    case %s: {
        %sParent** parent =
        reinterpret_cast<%sParent**>(&gParentActor);
        *parent = new %sParent();
        (*parent)->Open(transport, child);
        return (*parent)->Main();
        }
'''% (t, t, t, t) for t in unittests ])

    child_delete_cases = '\n'.join([
'''    case %s: {
           delete reinterpret_cast<%sChild*>(gChildActor);
           return;
       }
'''% (t, t) for t in unittests+extras ])


    child_init_cases = '\n'.join([
'''    case %s: {
        %sChild** child =
            reinterpret_cast<%sChild**>(&gChildActor);
        *child = new %sChild();
        (*child)->Open(transport, parent, worker);
        return;
    }
'''% (t, t, t, t) for t in unittests+extras ])

    templatefile = open(template, 'r')
    sys.stdout.write(
        string.Template(templatefile.read()).substitute(
            INCLUDES=includes,
            ENUM_VALUES=enum_values, LAST_ENUM=last_enum,
            STRING_TO_ENUMS=string_to_enums,
            ENUM_TO_STRINGS=enum_to_strings,
            PARENT_DELETE_CASES=parent_delete_cases,
            PARENT_MAIN_CASES=parent_main_cases,
            CHILD_DELETE_CASES=child_delete_cases,
            CHILD_INIT_CASES=child_init_cases))
    templatefile.close()

if __name__ == '__main__':
    main(sys.argv)
