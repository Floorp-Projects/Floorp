# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to capture the content of config.status-generated
# files and subsequently restore their timestamp if they haven't changed.

import os
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

def dump(dump_file, shell):
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
