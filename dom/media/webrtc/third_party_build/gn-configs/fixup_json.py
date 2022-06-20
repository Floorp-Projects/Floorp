#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import re
import sys


def main():
    # Check and retrieve command-line arguments
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)  # Return a non-zero value to indicate abnormal termination
    filein = sys.argv[1]

    if not os.path.isfile(filein):
        print("error: {} does not exist".format(filein))
        sys.exit(1)

    f = open(filein, "r")
    file = f.read()

    # In practice, almost all of the entries in the cflags section have no affect
    # on the moz.build output files when running ./mach build-backend -b GnMozbuildWriter
    # There are few exceptions which do: -msse2, -mavx2, -mfma, -fobjc-arc
    # However, since we're really concerned about removing differences between development
    # machines, we only need remove the reference to osx sdk.  Removing it doesn't change
    # the generated moz.build files and makes diffs much easier to see.
    file = re.sub(
        r' *"-isysroot",\n *".*/Contents/Developer/Platforms/MacOSX\.platform/Developer/SDKs/'
        'MacOSX([0-9]+\.[0-9]+)?\.sdk",\n',
        r"",
        file,
    )
    f.close()

    f = open(filein, "w")
    f.write(file)
    f.close()


if __name__ == "__main__":
    main()
