# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for populating the virtualenv environment for
# Mozilla's build system. It is typically called as part of configure.

from __future__ import with_statement
import os.path
import subprocess
import sys
import distutils.sysconfig

def populate_virtualenv(top_source_directory, manifest_filename):
    """Populate the virtualenv from the contents of a manifest.

    The manifest file consists of colon-delimited fields. The first field
    specifies the action. The remaining fields are arguments to that action.
    The following actions are supported:

      setup.py -- Invoke setup.py for a package. Expects the arguments:
        1. relative path directory containing setup.py.
        2. argument(s) to setup.py. e.g. "develop". Each program argument is
           delimited by a colon. Arguments with colons are not yet supported.

    Note that the Python interpreter running this function should be the one
    from the virtualenv. If it is the system Python or if the environment is
    not configured properly, packages could be installed into the wrong place.
    This is how virtualenv's work.
    """
    packages = []
    fh = open(manifest_filename, 'rU')
    for line in fh:
        packages.append(line.rstrip().split(':'))
    fh.close()

    for package in packages:
        if package[0] == 'setup.py':
            assert len(package) >= 2

            call_setup(os.path.join(top_source_directory, package[1]),
                package[2:])
        if package[0].endswith('.pth'):
            assert len(package) == 2

            with open(os.path.join(distutils.sysconfig.get_python_lib(), package[0]), 'a') as f:
                f.write("%s\n" % os.path.join(top_source_directory, package[1]))

def call_setup(directory, arguments):
    """Calls setup.py in a directory."""
    setup = os.path.join(directory, 'setup.py')

    program = [sys.executable, setup]
    program.extend(arguments)

    # We probably could call the contents of this file inside the context of
    # this interpreter using execfile() or similar. However, if global
    # variables like sys.path are adjusted, this could cause all kinds of
    # havoc. While this may work, invoking a new process is safer.
    result = subprocess.call(program, cwd=directory)

    if result != 0:
        raise Exception('Error installing package: %s' % directory)

# configure invokes us with /path/to/topsrcdir and /path/to/manifest
if __name__ == '__main__':
    assert len(sys.argv) == 3

    populate_virtualenv(sys.argv[1], sys.argv[2])
    sys.exit(0)
