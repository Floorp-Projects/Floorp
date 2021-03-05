#!/usr/bin/env python2.7
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to create and manipulate archives containing
# files necessary to build Firefox on Windows (referred to as the
# "Windows toolchain").
#
# When updating behavior of this script, remember to update the docs
# in ``build/docs/toolchains.rst``.

from __future__ import absolute_import, unicode_literals

import hashlib
import os
import sys

import six

from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter
import mozpack.path as mozpath

SDK_RELEASE = "10.0.17134.0"

PATTERNS = [
    {
        "srcdir": "%(vs_path)s/DIA SDK",
        "dstdir": "DIA SDK",
        "files": [
            {
                "pattern": "bin/**",
                "ignore": ("bin/arm/**",),
            },
            {
                "pattern": "idl/**",
            },
            {
                "pattern": "include/**",
            },
            {
                "pattern": "lib/**",
                "ignore": ("lib/arm/**",),
            },
        ],
    },
    {
        "srcdir": "%(vs_path)s/VC/Tools/MSVC/14.16.27023",
        "dstdir": "VC",
        "files": [
            # ATL is needed by Breakpad.
            {
                "pattern": "atlmfc/include/**",
            },
            {
                "pattern": "atlmfc/lib/arm64/atls.*",
            },
            {
                "pattern": "atlmfc/lib/x64/atls.*",
            },
            {
                "pattern": "atlmfc/lib/x86/atls.*",
            },
            # ARM64 PGO-instrumented builds require ARM64 pgort140.dll.
            {
                "pattern": "bin/arm64/pgort140.dll",
            },
            {
                "pattern": "bin/Hostx64/**",
            },
            # 32-bit PGO-instrumented builds require 32-bit pgort140.dll.
            {
                "pattern": "bin/Hostx86/x86/pgort140.dll",
            },
            {
                "pattern": "include/**",
            },
            {
                "pattern": "lib/**",
                "ignore": (
                    "lib/arm64/store/**",
                    "lib/onecore/**",
                    "lib/x64/store/**",
                    "lib/x86/store/**",
                ),
            },
        ],
    },
    {
        "srcdir": "%(vs_path)s/VC/Redist/MSVC/14.16.27012",
        "dstdir": "VC/redist",
        "files": [
            {
                "pattern": "arm64/Microsoft.VC141.CRT/**",
            },
            {
                "pattern": "x64/Microsoft.VC141.CRT/**",
            },
            {
                "pattern": "x86/Microsoft.VC141.CRT/**",
            },
        ],
    },
    {
        "srcdir": "%(sdk_path)s",
        "dstdir": "SDK",
        "files": [
            {
                "pattern": "bin/%s/x64/**" % SDK_RELEASE,
            },
            {
                "pattern": "Include/%s/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/ucrt/arm64/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/ucrt/x64/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/ucrt/x86/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/um/arm64/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/um/x64/**" % SDK_RELEASE,
            },
            {
                "pattern": "Lib/%s/um/x86/**" % SDK_RELEASE,
            },
            {
                "pattern": "Redist/D3D/**",
            },
            {
                "pattern": "Redist/ucrt/DLLs/x64/**",
            },
            {
                "pattern": "Redist/ucrt/DLLs/x86/**",
            },
        ],
    },
]


def find_vs_paths():
    """Resolve source locations of files.

    Returns a 2-tuple of (Visual Studio Path, SDK Path).
    """
    pf = os.environ.get("ProgramFiles(x86)")
    if not pf:
        raise Exception(
            'No "ProgramFiles(x86)" environment variable. '
            "Not running on 64-bit Windows?"
        )

    vs_path = os.path.join(pf, "Microsoft Visual Studio", "2017", "Community")
    if not os.path.exists(vs_path):
        raise Exception(
            "%s does not exist; Visual Studio 2017 not installed?" % vs_path
        )

    sdk_path = os.path.join(pf, "Windows Kits", "10")
    if not os.path.exists(sdk_path):
        raise Exception("%s does not exist; Windows 10 SDK not installed?" % sdk_path)

    sdk_fullver_path = os.path.join(sdk_path, "Include", SDK_RELEASE)
    if not os.path.exists(sdk_fullver_path):
        raise Exception(
            "%s does not exist; Wrong SDK version installed?" % sdk_fullver_path
        )

    return vs_path, sdk_path


def resolve_files():
    """Resolve the files that constitute a standalone toolchain.

    This is a generator of (dest path, file) where the destination
    path is relative and the file instance is a BaseFile from mozpack.
    """
    vs_path, sdk_path = find_vs_paths()

    for entry in PATTERNS:
        fullpath = entry["srcdir"] % {
            "vs_path": vs_path,
            "sdk_path": sdk_path,
        }
        for pattern in entry["files"]:
            finder = FileFinder(fullpath, ignore=pattern.get("ignore", []))
            for p, f in finder.find(pattern["pattern"]):
                dstpath = "%s/%s" % (entry["dstdir"], p)
                yield dstpath.encode("utf-8"), f


def resolve_files_and_hash(manifest):
    """Resolve files and hash their data.

    This is a generator of 3-tuples of (relpath, data, mode).

    As data is read, the manifest is populated with metadata.
    Keys are set to the relative file path. Values are 2-tuples
    of (data length, sha-256).
    """
    assert manifest == {}
    for p, f in resolve_files():
        data = f.read()

        sha256 = hashlib.sha256()
        sha256.update(data)
        manifest[p] = (len(data), sha256.hexdigest().encode("utf-8"))

        yield p, data, f.mode


def format_manifest(manifest):
    """Return formatted SHA-256 manifests as a byte strings."""
    sha256_lines = []
    for path, (length, sha256) in sorted(manifest.items()):
        sha256_lines.append(b"%s\t%d\t%s" % (sha256, length, path))

    # Trailing newline.
    sha256_lines.append(b"")

    return b"\n".join(sha256_lines)


def write_zip(zip_path, prefix=None):
    """Write toolchain data to a zip file."""
    prefix = six.ensure_binary(prefix, encoding="utf-8")

    with JarWriter(file=zip_path, compress_level=5) as zip:
        manifest = {}
        for p, data, mode in resolve_files_and_hash(manifest):
            print(p)
            if prefix:
                p = mozpath.join(prefix, p)

            zip.add(p, data, mode=mode)

        sha256_manifest = format_manifest(manifest)

        sdk_path = b"SDK_VERSION"
        sha256_path = b"MANIFEST.SHA256"
        if prefix:
            sdk_path = mozpath.join(prefix, sdk_path)
            sha256_path = mozpath.join(prefix, sha256_path)

        zip.add(sdk_path, SDK_RELEASE.encode("utf-8"))
        zip.add(sha256_path, sha256_manifest)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: %s create-zip <path-prefix>" % sys.argv[0])
        sys.exit(1)

    assert sys.argv[1] == "create-zip"
    prefix = os.path.basename(sys.argv[2])
    destzip = "%s.zip" % sys.argv[2]
    write_zip(destzip, prefix=prefix)

    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    sha512 = hashlib.sha512()

    with open(destzip, "rb") as fh:
        data = fh.read()
        sha1.update(data)
        sha256.update(data)
        sha512.update(data)

    print("Hashes of %s (size=%d)" % (destzip, len(data)))
    print("SHA-1:   %s" % sha1.hexdigest())
    print("SHA-256: %s" % sha256.hexdigest())
    print("SHA-512: %s" % sha512.hexdigest())
