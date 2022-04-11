#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from vsdownload import (
    downloadPackages,
    extractPackages,
)
from pathlib import Path
from tempfile import TemporaryDirectory
from zstandard import ZstdCompressor
import argparse
import os
import tarfile
import yaml


def tzstd_path(path):
    path = Path(path)
    if path.suffix not in (".zst", ".zstd") or Path(path.stem).suffix != ".tar":
        raise ValueError("F")
    return path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Download and build a Visual Studio artifact"
    )
    parser.add_argument("manifest", help="YAML manifest of the contents to download")
    parser.add_argument(
        "-o", dest="output", type=tzstd_path, required=True, help="Output file"
    )
    args = parser.parse_args()

    with open(args.manifest) as f:
        selected = yaml.safe_load(f.read())
    with TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        dl_cache = tmpdir / "cache"
        downloadPackages(selected, dl_cache)
        unpacked = tmpdir / "unpack"
        extractPackages(selected, dl_cache, unpacked)
        stem = Path(Path(args.output.stem).stem)
        # Create an archive containing all the paths in lowercase form for
        # cross-compiles.
        with open(args.output, "wb") as f:
            with ZstdCompressor().stream_writer(f) as z:
                with tarfile.open(mode="w|", fileobj=z) as tar:
                    for subpath, dest in (
                        ("VC", "vc"),
                        ("Program Files/Windows Kits/10", "windows kits/10"),
                        ("DIA SDK", "dia sdk"),
                    ):
                        subpath = unpacked / subpath
                        dest = Path(dest)
                        for root, dirs, files in os.walk(subpath):
                            relpath = Path(root).relative_to(subpath)
                            for f in files:
                                path = Path(root) / f
                                info = tar.gettarinfo(path)
                                with open(path, "rb") as fh:
                                    info.name = str(stem / dest / relpath / f).lower()
                                    # Set executable flag on .exe files, the Firefox build
                                    # system wants it.
                                    if info.name.endswith(".exe"):
                                        info.mode |= (info.mode & 0o444) >> 2
                                    print("Adding", info.name)
                                    tar.addfile(info, fh)
