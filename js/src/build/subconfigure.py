# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to capture the content of config.status-generated
# files and subsequently restore their timestamp if they haven't changed.

import os
import re
import subprocess
import sys
import pickle

class File(object):
    def __init__(self, path):
        self._path = path
        self._content = open(path, 'rb').read()
        stat = os.stat(path)
        self._times = (stat.st_atime, stat.st_mtime)

    def update_time(self):
        '''If the file hasn't changed since the instance was created,
           restore its old modification time.'''
        if not os.path.exists(self._path):
            return
        if open(self._path, 'rb').read() == self._content:
            os.utime(self._path, self._times)


# As defined in the various sub-configures in the tree
PRECIOUS_VARS = set([
    'build_alias',
    'host_alias',
    'target_alias',
    'CC',
    'CFLAGS',
    'LDFLAGS',
    'LIBS',
    'CPPFLAGS',
    'CPP',
    'CCC',
    'CXXFLAGS',
    'CXX',
    'CCASFLAGS',
    'CCAS',
])


# Autoconf, in some of the sub-configures used in the tree, likes to error
# out when "precious" variables change in value. The solution it gives to
# straighten things is to either run make distclean or remove config.cache.
# There's no reason not to do the latter automatically instead of failing,
# doing the cleanup (which, on buildbots means a full clobber), and
# restarting from scratch.
def maybe_clear_cache():
    comment = re.compile(r'^\s+#')
    cache = {}
    with open('config.cache') as f:
        for line in f.readlines():
            if not comment.match(line) and '=' in line:
                key, value = line.split('=', 1)
                cache[key] = value
    for precious in PRECIOUS_VARS:
        entry = 'ac_cv_env_%s_value' % precious
        if entry in cache and (not precious in os.environ or os.environ[precious] != cache[entry]):
            os.remove('config.cache')
            return


def dump(dump_file, shell):
    if os.path.exists('config.cache'):
        maybe_clear_cache()
    if not os.path.exists('config.status'):
        if os.path.exists(dump_file):
            os.remove(dump_file)
        return

    config_files = [File('config.status')]

    # Scan the config.status output for information about configuration files
    # it generates.
    config_status_output = subprocess.check_output(
        [shell, '-c', './config.status --help'],
        stderr=subprocess.STDOUT).splitlines()
    state = None
    for line in config_status_output:
        if line.startswith('Configuration') and line.endswith(':'):
            state = 'config'
        elif not line.startswith(' '):
            state = None
        elif state == 'config':
            for f in (couple.split(':')[0] for couple in line.split()):
                if os.path.isfile(f):
                    config_files.append(File(f))

    with open(dump_file, 'wb') as f:
        pickle.dump(config_files, f)


def adjust(dump_file):
    if not os.path.exists(dump_file):
        return

    config_files = []

    try:
        with open(dump_file, 'rb') as f:
            config_files = pickle.load(f)
    except Exception:
        pass

    for f in config_files:
        f.update_time()

    os.remove(dump_file)


CONFIG_DUMP = 'config_files.pkl'

if __name__ == '__main__':
    if sys.argv[1] == 'dump':
        dump(CONFIG_DUMP, sys.argv[2])
    elif sys.argv[1] == 'adjust':
        adjust(CONFIG_DUMP)
