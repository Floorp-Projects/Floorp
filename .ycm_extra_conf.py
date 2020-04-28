# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function
import json
import os
import shlex
import subprocess
import sys

old_bytecode = sys.dont_write_bytecode
sys.dont_write_bytecode = True

path = os.path.abspath(os.path.join(os.path.dirname(__file__), 'mach'))

# If mach is not here, we're on the objdir go to the srcdir.
if not os.path.exists(path):
    with open(os.path.join(os.path.dirname(__file__), 'mozinfo.json')) as info:
        config = json.loads(info.read())
    path = os.path.join(config['topsrcdir'], 'mach')

sys.dont_write_bytecode = old_bytecode

def _is_likely_cpp_header(filename):
    if not filename.endswith('.h'):
        return False

    if filename.endswith('Inlines.h') or filename.endswith('-inl.h'):
        return True

    cpp_file = filename[:-1] + 'cpp'
    return os.path.exists(cpp_file)


def Settings(**kwargs):
    if kwargs[ 'language' ] == 'cfamily':
        return FlagsForFile(kwargs['filename'])
    return {}


def FlagsForFile(filename):
    output = subprocess.check_output([path, 'compileflags', filename])
    output = output.decode('utf-8')

    flag_list = shlex.split(output)

    # This flag is added by Fennec for android build and causes ycmd to fail to parse the file.
    # Removing this flag is a workaround until ycmd starts to handle this flag properly.
    # https://github.com/Valloric/YouCompleteMe/issues/1490
    final_flags = [x for x in flag_list if not x.startswith('-march=armv')]

    if _is_likely_cpp_header(filename):
        final_flags += ["-x", "c++"]

    return {
        'flags': final_flags,
        'do_cache': True
    }

if __name__ == '__main__':
    print(FlagsForFile(sys.argv[1]))
