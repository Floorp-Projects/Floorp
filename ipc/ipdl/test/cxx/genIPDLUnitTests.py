# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    parent_enabled_cases_proc = '\n'.join([
'''    case %s: {
        if (!%sParent::RunTestInProcesses()) {
            passed("N/A to proc");
            DeferredParentShutdown();
            return;
        }
        break;
       }
''' % (t, t) for t in unittests ])

    parent_main_cases_proc = '\n'.join([
'''    case %s: {
        %sParent** parent =
        reinterpret_cast<%sParent**>(&gParentActor);
        *parent = new %sParent();
        (*parent)->Open(transport, child);
        return (*parent)->Main();
        }
'''% (t, t, t, t) for t in unittests ])

    parent_enabled_cases_thread = '\n'.join([
'''    case %s: {
        if (!%sParent::RunTestInThreads()) {
            passed("N/A to threads");
            DeferredParentShutdown();
            return;
        }
        break;
       }
''' % (t, t) for t in unittests ])

    parent_main_cases_thread = '\n'.join([
'''    case %s: {
        %sParent** parent =
        reinterpret_cast<%sParent**>(&gParentActor);
        *parent = new %sParent();

        %sChild** child =
        reinterpret_cast<%sChild**>(&gChildActor);
        *child = new %sChild();

        mozilla::ipc::AsyncChannel *parentChannel = (*parent)->GetIPCChannel();
        mozilla::ipc::AsyncChannel *childChannel = (*child)->GetIPCChannel();
        mozilla::ipc::AsyncChannel::Side parentSide = 
            mozilla::ipc::AsyncChannel::Parent;

        (*parent)->Open(childChannel, childMessageLoop, parentSide);
        return (*parent)->Main();
        }
'''% (t, t, t, t, t, t, t) for t in unittests ])

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
            PARENT_ENABLED_CASES_PROC=parent_enabled_cases_proc,
            PARENT_MAIN_CASES_PROC=parent_main_cases_proc,
            PARENT_ENABLED_CASES_THREAD=parent_enabled_cases_thread,
            PARENT_MAIN_CASES_THREAD=parent_main_cases_thread,
            CHILD_DELETE_CASES=child_delete_cases,
            CHILD_INIT_CASES=child_init_cases))
    templatefile.close()

if __name__ == '__main__':
    main(sys.argv)
