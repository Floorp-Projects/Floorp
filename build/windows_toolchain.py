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

from mozpack.files import (
    FileFinder,
)
from mozpack.mozjar import (
    JarWriter,
)
import mozpack.path as mozpath


# mozpack.match patterns for files under "Microsoft Visual Studio 14.0".
VS_PATTERNS = [
    {
        'pattern': 'DIA SDK/bin/**',
        'ignore': (
            'DIA SDK/bin/arm/**',
        ),
    },
    {
        'pattern': 'DIA SDK/idl/**',
    },
    {
        'pattern': 'DIA SDK/include/**',
    },
    {
        'pattern': 'DIA SDK/lib/**',
        'ignore': (
            'DIA SDK/lib/arm/**',
        ),
    },
    # ATL is needed by Breakpad.
    {
        'pattern': 'VC/atlmfc/include/**',
    },
    {
        'pattern': 'VC/atlmfc/lib/atls.*',
    },
    {
        'pattern': 'VC/atlmfc/lib/amd64/atls.*',
    },
    {
        'pattern': 'VC/bin/**',
        # We only care about compiling on amd64 for amd64 or x86 targets.
        'ignore': (
            'VC/bin/amd64_arm/**',
            'VC/bin/arm/**',
            'VC/bin/x86_arm/**',
            'VC/bin/x86_amd64/**',
        ),
    },
    {
        'pattern': 'VC/include/**',
    },
    {
        'pattern': 'VC/lib/**',
        'ignore': (
            'VC/lib/arm/**',
            'VC/lib/onecore/**',
            'VC/lib/store/**',
        ),
    },
    {
        'pattern': 'VC/redist/x64/Microsoft.VC140.CRT/**',
    },
    {
        'pattern': 'VC/redist/x86/Microsoft.VC140.CRT/**',
    },
]

SDK_RELEASE = '10.0.10586.0'

# Files from the Windows 10 SDK to install.
SDK_PATTERNS = [
    {
        'pattern': 'bin/x64/**',
    },
    {
        'pattern': 'Include/%s/**' % SDK_RELEASE,
    },
    {
        'pattern': 'Lib/%s/ucrt/x64/**' % SDK_RELEASE,
    },
    {
        'pattern': 'Lib/%s/ucrt/x86/**' % SDK_RELEASE,
    },
    {
        'pattern': 'Lib/%s/um/x64/**' % SDK_RELEASE,
    },
    {
        'pattern': 'Lib/%s/um/x86/**' % SDK_RELEASE,
    },
    {
        'pattern': 'Redist/D3D/**',
    },
    {
        'pattern': 'Redist/ucrt/DLLs/x64/**',
    },
    {
        'pattern': 'Redist/ucrt/DLLs/x86/**',
    },
]


def find_vs_paths():
    """Resolve source locations of files.

    Returns a 2-tuple of (Visual Studio Path, SDK Path).
    """
    pf = os.environ.get('ProgramFiles(x86)')
    if not pf:
        raise Exception('No "ProgramFiles(x86)" environment variable. '
                        'Not running on 64-bit Windows?')

    vs_path = os.path.join(pf, 'Microsoft Visual Studio 14.0')
    if not os.path.exists(vs_path):
        raise Exception('%s does not exist; Visual Studio 2015 not installed?' %
                        vs_path)

    sdk_path = os.path.join(pf, 'Windows Kits', '10')
    if not os.path.exists(sdk_path):
        raise Exception('%s does not exist; Windows 10 SDK not installed?' %
                        sdk_path)

    return vs_path, sdk_path


def resolve_files():
    """Resolve the files that constitute a standalone toolchain.

    This is a generator of (dest path, file) where the destination
    path is relative and the file instance is a BaseFile from mozpack.
    """
    vs_path, sdk_path = find_vs_paths()

    for entry in VS_PATTERNS:
        finder = FileFinder(vs_path, find_executables=False,
                            ignore=entry.get('ignore', []))
        for p, f in finder.find(entry['pattern']):
            assert p.startswith(('VC/', 'DIA SDK/'))

            yield p.encode('utf-8'), f

    for entry in SDK_PATTERNS:
        finder = FileFinder(sdk_path, find_executables=False,
                            ignore=entry.get('ignore', []))
        for p, f in finder.find(entry['pattern']):
            relpath = 'SDK/%s' % p

            yield relpath.encode('utf-8'), f


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
        manifest[p] = (len(data), sha256.hexdigest())

        yield p, data, f.mode


def format_manifest(manifest):
    """Return formatted SHA-256 manifests as a byte strings."""
    sha256_lines = []
    for path, (length, sha256) in sorted(manifest.items()):
        sha256_lines.append(b'%s\t%d\t%s' % (sha256, length, path))

    # Trailing newline.
    sha256_lines.append(b'')

    return b'\n'.join(sha256_lines)


def write_zip(zip_path, prefix=None):
    """Write toolchain data to a zip file."""
    if isinstance(prefix, unicode):
        prefix = prefix.encode('utf-8')

    with JarWriter(file=zip_path, optimize=False, compress=5) as zip:
        manifest = {}
        for p, data, mode in resolve_files_and_hash(manifest):
            print(p)
            if prefix:
                p = mozpath.join(prefix, p)

            zip.add(p, data, mode=mode)

        sha256_manifest = format_manifest(manifest)

        sdk_path = b'SDK_VERSION'
        sha256_path = b'MANIFEST.SHA256'
        if prefix:
            sdk_path = mozpath.join(prefix, sdk_path)
            sha256_path = mozpath.join(prefix, sha256_path)

        zip.add(sdk_path, SDK_RELEASE.encode('utf-8'))
        zip.add(sha256_path, sha256_manifest)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: %s create-zip <path-prefix>' % sys.argv[0])
        sys.exit(1)

    assert sys.argv[1] == 'create-zip'
    prefix = os.path.basename(sys.argv[2])
    destzip = '%s.zip' % sys.argv[2]
    write_zip(destzip, prefix=prefix)

    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    sha512 = hashlib.sha512()

    with open(destzip, 'rb') as fh:
        data = fh.read()
        sha1.update(data)
        sha256.update(data)
        sha512.update(data)

    print('Hashes of %s (size=%d)' % (destzip, len(data)))
    print('SHA-1:   %s' % sha1.hexdigest())
    print('SHA-256: %s' % sha256.hexdigest())
    print('SHA-512: %s' % sha512.hexdigest())
