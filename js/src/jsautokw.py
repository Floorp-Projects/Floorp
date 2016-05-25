# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import os
import sys
import subprocess

def main(output, exe):
    # moz.build passes in the exe name without any path, so to run it we need to
    # prepend the './'
    run_exe = exe if os.path.isabs(exe) else './%s' % exe

    # Use universal_newlines so everything is '\n', which gets converted to
    # '\r\n' when writing out the file in Windows.
    data = subprocess.check_output([run_exe], universal_newlines=True)
    output.write(data)

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
