# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import buildconfig
import mozpack.path as mozpath
import os
import six
import subprocess
import pytoml


# Try to read the package name or otherwise assume same name as the crate path.
def _get_crate_name(crate_path):
    try:
        with open(mozpath.join(crate_path, "Cargo.toml")) as f:
            return pytoml.load(f)["package"]["name"]
    except Exception:
        return mozpath.basename(crate_path)


CARGO_LOCK = mozpath.join(buildconfig.topsrcdir, "Cargo.lock")
CARGO_TOML = mozpath.join(buildconfig.topsrcdir, "Cargo.toml")


def _run_process(args):
    env = os.environ.copy()
    env["CARGO"] = str(buildconfig.substs["CARGO"])
    env["RUSTC"] = str(buildconfig.substs["RUSTC"])

    p = subprocess.Popen(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    stdout, stderr = p.communicate()
    stdout = six.ensure_text(stdout)
    stderr = six.ensure_text(stderr)
    if p.returncode != 0:
        print(stdout)
        print(stderr)
    return (stdout, p.returncode)


def generate_metadata(output, cargo_config):
    stdout, returncode = _run_process(
        [
            buildconfig.substs["CARGO"],
            "metadata",
            "--frozen",
            "--all-features",
            "--format-version",
            "1",
            "--manifest-path",
            CARGO_TOML,
        ]
    )

    if returncode != 0:
        return returncode

    output.write(stdout)

    # This is not quite accurate, but cbindgen only cares about a subset of the
    # data which, when changed, causes these files to change.
    return set([CARGO_LOCK, CARGO_TOML])


def generate(output, metadata_path, cbindgen_crate_path, *in_tree_dependencies):
    stdout, returncode = _run_process(
        [
            buildconfig.substs["CBINDGEN"],
            buildconfig.topsrcdir,
            "--lockfile",
            CARGO_LOCK,
            "--crate",
            _get_crate_name(cbindgen_crate_path),
            "--metadata",
            metadata_path,
            "--cpp-compat",
        ]
    )

    if returncode != 0:
        return returncode

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
