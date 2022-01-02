# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
from buildconfig import substs


def main(output, *other_libs):
    output.close()
    # ar doesn't like it when the file exists beforehand.
    os.unlink(output.name)
    libs = [output.name]
    parent = os.path.dirname(output.name)
    libs.extend(os.path.join(parent, l) for l in other_libs)
    for lib in libs:
        result = subprocess.run(
            [substs["AR"]] + [f.replace("$@", lib) for f in substs["AR_FLAGS"]]
        )
        if result.returncode != 0:
            return result.returncode
    return 0
