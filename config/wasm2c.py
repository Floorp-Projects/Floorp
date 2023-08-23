# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess


def wasm2c(output, wasm2c_bin, wasm_lib):
    output.close()
    module_name = os.path.basename(os.path.splitext(wasm_lib)[0])
    return subprocess.run(
        [wasm2c_bin, "-n", module_name, "-o", output.name, wasm_lib]
    ).returncode
