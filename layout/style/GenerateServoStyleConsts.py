# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import buildconfig
import mozpack.path as mozpath
import os
import subprocess

STYLE = mozpath.join(buildconfig.topsrcdir, "servo", "components", "style")
CARGO_LOCK = mozpath.join(buildconfig.topsrcdir, "Cargo.lock")

def generate(output, cbindgen_toml_path):
    env = os.environ.copy()
    env['CARGO'] = str(buildconfig.substs['CARGO'])
    p = subprocess.Popen([
        buildconfig.substs['CBINDGEN'],
        mozpath.join(buildconfig.topsrcdir, "toolkit", "library", "rust"),
        "--lockfile",
        CARGO_LOCK,
        "--crate",
        "style"
    ], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    stdout, stderr = p.communicate()
    if p.returncode == 0:
        output.write(stdout)
    else:
        print("cbindgen failed: %s" % stderr)

    deps = set()
    deps.add(CARGO_LOCK)
    for path, dirs, files in os.walk(STYLE):
        for file in files:
            if os.path.splitext(file)[1] == ".rs":
                deps.add(mozpath.join(path, file))
    return deps
