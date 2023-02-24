# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from mach.decorators import Command, CommandArgument
from mozbuild.util import mkdir


def get_test_parser():
    import runtests

    return runtests.get_parser


@Command(
    "webidl-example",
    category="misc",
    description="Generate example files for a WebIDL interface.",
)
@CommandArgument(
    "interface", nargs="+", help="Interface(s) whose examples to generate."
)
def webidl_example(command_context, interface):
    from mozwebidlcodegen import create_build_system_manager

    manager = create_build_system_manager()
    for i in interface:
        manager.generate_example_files(i)


@Command(
    "webidl-parser-test",
    category="testing",
    parser=get_test_parser,
    description="Run WebIDL tests (Interface Browser parser).",
)
def webidl_test(command_context, **kwargs):
    sys.path.insert(0, os.path.join(command_context.topsrcdir, "other-licenses", "ply"))

    # Ensure the topobjdir exists. On a Taskcluster test run there won't be
    # an objdir yet.
    mkdir(command_context.topobjdir)

    # Make sure we drop our cached grammar bits in the objdir, not
    # wherever we happen to be running from.
    os.chdir(command_context.topobjdir)

    if kwargs["verbose"] is None:
        kwargs["verbose"] = False

    # Now we're going to create the cached grammar file in the
    # objdir.  But we're going to try loading it as a python
    # module, so we need to make sure the objdir is in our search
    # path.
    sys.path.insert(0, command_context.topobjdir)

    import runtests

    return runtests.run_tests(kwargs["tests"], verbose=kwargs["verbose"])
