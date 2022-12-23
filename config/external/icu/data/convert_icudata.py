# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess

import buildconfig


def main(output, data_file):
    output.close()
    subprocess.run(
        [
            os.path.join(buildconfig.topobjdir, "dist", "host", "bin", "icupkg"),
            "-tb",
            data_file,
            output.name,
        ]
    )
