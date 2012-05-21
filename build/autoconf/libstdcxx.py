#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# This script find the version of libstdc++ and prints it as single number
# with 8 bits per element. For example, GLIBCXX_3.4.10 becomes
# 3 << 16 | 4 << 8 | 10 = 197642. This format is easy to use
# in the C preprocessor.

# We find out both the host and target versions. Since the output
# will be used from shell, we just print the two assignments and evaluate
# them from shell.

import os
import subprocess
import re
import sys

re_for_ld = re.compile('.*\((.*)\).*')

def parse_readelf_line(x):
    """Return the version from a readelf line that looks like:
    0x00ec: Rev: 1  Flags: none  Index: 8  Cnt: 2  Name: GLIBCXX_3.4.6
    """
    return x.split(':')[-1].split('_')[-1].strip()

def parse_ld_line(x):
    """Parse a line from the output of ld -t. The output of gold is just
    the full path, gnu ld prints "-lstdc++ (path)".
    """
    t = re_for_ld.match(x)
    if t:
        return t.groups()[0].strip()
    return x.strip()

def split_ver(v):
    """Covert the string '1.2.3' into the list [1,2,3]
    """
    return [int(x) for x in v.split('.')]

def cmp_ver(a, b):
    """Compare versions in the form 'a.b.c'
    """
    for (i, j) in zip(split_ver(a), split_ver(b)):
        if i != j:
            return i - j
    return 0

def encode_ver(v):
    """Encode the version as a single number.
    """
    t = split_ver(v)
    return t[0] << 16 | t[1] << 8 | t[2]

def find_version(e):
    """Given the value of environment variable CXX or HOST_CXX, find the
    version of the libstdc++ it uses.
    """
    args = e.split()
    args +=  ['-shared', '-Wl,-t']
    p = subprocess.Popen(args, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    candidates = [x for x in p.stdout if 'libstdc++.so' in x]
    assert len(candidates) == 1
    libstdcxx = parse_ld_line(candidates[-1])

    p = subprocess.Popen(['readelf', '-V', libstdcxx], stdout=subprocess.PIPE)
    versions = [parse_readelf_line(x)
                for x in p.stdout.readlines() if 'Name: GLIBCXX' in x]
    last_version = sorted(versions, cmp = cmp_ver)[-1]
    return encode_ver(last_version)

if __name__ == '__main__':
    if os.uname()[0] == 'Darwin':
        sdk_dir = os.environ['MACOS_SDK_DIR']
        if 'MacOSX10.5.sdk' in sdk_dir:
            target_ver = 0
            host_ver = 0
        else:
            target_ver = encode_ver('3.4.9')
            host_ver = encode_ver('3.4.9')
    else:
        cxx_env = os.environ['CXX']
        target_ver = find_version(cxx_env)
        host_cxx_env = os.environ.get('HOST_CXX', cxx_env)
        host_ver = find_version(host_cxx_env)

    print 'MOZ_LIBSTDCXX_TARGET_VERSION=%s' % target_ver
    print 'MOZ_LIBSTDCXX_HOST_VERSION=%s' % host_ver
