# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import buildconfig
import mozpack.path as mozpath
import os
import subprocess
import pytoml

# Try to read the package name or otherwise assume same name as the crate path.
def _get_crate_name(crate_path):
    try:
        with open(mozpath.join(crate_path, "Cargo.toml")) as f:
          return pytoml.load(f)["package"]["name"]
    except:
        return mozpath.basename(crate_path)

CARGO_LOCK = mozpath.join(buildconfig.topsrcdir, "Cargo.lock")

def generate(output, cbindgen_crate_path, *in_tree_dependencies):
    env = os.environ.copy()
    env['CARGO'] = str(buildconfig.substs['CARGO'])

    p = subprocess.Popen([
        buildconfig.substs['CBINDGEN'],
        mozpath.join(buildconfig.topsrcdir, "toolkit", "library", "rust"),
        "--lockfile",
        CARGO_LOCK,
        "--crate",
        _get_crate_name(cbindgen_crate_path)
    ], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    stdout, stderr = p.communicate()
    if p.returncode != 0:
        print(stdout)
        print(stderr)
        return p.returncode

    output.write(stdout)

    deps = set()
    deps.add(CARGO_LOCK)
    deps.add(mozpath.join(cbindgen_crate_path, "cbindgen.toml"))
    for directory in in_tree_dependencies + (cbindgen_crate_path,):
        for path, dirs, files in os.walk(directory):
            for file in files:
                if os.path.splitext(file)[1] == ".rs":
                    deps.add(mozpath.join(path, file))

    return deps
