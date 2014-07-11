# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to capture the content of config.status-generated
# files and subsequently restore their timestamp if they haven't changed.

import argparse
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

    @property
    def path(self):
        return self._path

    @property
    def mtime(self):
        return self._times[1]

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
def maybe_clear_cache(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', type=str)
    parser.add_argument('--host', type=str)
    parser.add_argument('--build', type=str)
    args, others = parser.parse_known_args(args)
    env = dict(os.environ)
    for kind in ('target', 'host', 'build'):
        arg = getattr(args, kind)
        if arg is not None:
            env['%s_alias' % kind] = arg
    # configure can take variables assignments in its arguments, and that
    # overrides whatever is in the environment.
    for arg in others:
        if arg[:1] != '-' and '=' in arg:
            key, value = arg.split('=', 1)
            env[key] = value

    comment = re.compile(r'^\s+#')
    cache = {}
    with open('config.cache') as f:
        for line in f:
            if not comment.match(line) and '=' in line:
                key, value = line.rstrip(os.linesep).split('=', 1)
                # If the value is quoted, unquote it
                if value[:1] == "'":
                    value = value[1:-1].replace("'\\''", "'")
                cache[key] = value
    for precious in PRECIOUS_VARS:
        # If there is no entry at all for that precious variable, then
        # its value is not precious for that particular configure.
        if 'ac_cv_env_%s_set' % precious not in cache:
            continue
        is_set = cache.get('ac_cv_env_%s_set' % precious) == 'set'
        value = cache.get('ac_cv_env_%s_value' % precious) if is_set else None
        if value != env.get(precious):
            print 'Removing config.cache because of %s value change from:' \
                % precious
            print '  %s' % (value if value is not None else 'undefined')
            print 'to:'
            print '  %s' % env.get(precious, 'undefined')
            os.remove('config.cache')
            return


def dump(dump_file, shell, args):
    if os.path.exists('config.cache'):
        maybe_clear_cache(args)
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


def adjust(dump_file, configure):
    if not os.path.exists(dump_file):
        return

    config_files = []

    try:
        with open(dump_file, 'rb') as f:
            config_files = pickle.load(f)
    except Exception:
        pass

    for f in config_files:
        # Still touch config.status if configure is newer than its original
        # mtime.
        if configure and os.path.basename(f.path) == 'config.status' and \
                os.path.getmtime(configure) > f.mtime:
            continue
        f.update_time()

    os.remove(dump_file)


CONFIG_DUMP = 'config_files.pkl'

if __name__ == '__main__':
    if sys.argv[1] == 'dump':
        dump(CONFIG_DUMP, sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'adjust':
        adjust(CONFIG_DUMP, sys.argv[2] if len(sys.argv) > 2 else None)
