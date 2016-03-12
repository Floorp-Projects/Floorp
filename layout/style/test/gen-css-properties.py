# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import sys
import subprocess

def main(output, css_properties, exe):
    data = subprocess.check_output([exe])
    with open(css_properties) as f:
        data += f.read()
    output.write(data)

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
