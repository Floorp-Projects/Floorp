# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to capture the content of config.status-generated
# files and subsequently restore their timestamp if they haven't changed.

import argparse
import errno
import os
import re
import subprocess
import sys
import pickle

import mozpack.path as mozpath

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

    @property
    def modified(self):
        '''Returns whether the file was modified since the instance was
        created. Result is memoized.'''
        if hasattr(self, '_modified'):
            return self._modified

        modified = True
        if os.path.exists(self._path):
            if open(self._path, 'rb').read() == self._content:
                modified = False
        self._modified = modified
        return modified

    def update_time(self):
        '''If the file hasn't changed since the instance was created,
           restore its old modification time.'''
        if not self.modified:
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
def maybe_clear_cache(data):
    env = dict(data['env'])
    for kind in ('target', 'host', 'build'):
        arg = data[kind]
        if arg is not None:
            env['%s_alias' % kind] = arg
    # configure can take variables assignments in its arguments, and that
    # overrides whatever is in the environment.
    for arg in data['args']:
        if arg[:1] != '-' and '=' in arg:
            key, value = arg.split('=', 1)
            env[key] = value

    comment = re.compile(r'^\s+#')
    cache = {}
    with open(data['cache-file']) as f:
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
            print 'Removing %s because of %s value change from:' \
                % (data['cache-file'], precious)
            print '  %s' % (value if value is not None else 'undefined')
            print 'to:'
            print '  %s' % env.get(precious, 'undefined')
            os.remove(data['cache-file'])
            return True
    return False


def split_template(s):
    """Given a "file:template" string, returns "file", "template". If the string
    is of the form "file" (without a template), returns "file", "file.in"."""
    if ':' in s:
        return s.split(':', 1)
    return s, '%s.in' % s


def get_config_files(data):
    config_status = mozpath.join(data['objdir'], 'config.status')
    if not os.path.exists(config_status):
        return [], []

    configure = mozpath.join(data['srcdir'], 'configure')
    config_files = []
    command_files = []

    # Scan the config.status output for information about configuration files
    # it generates.
    config_status_output = subprocess.check_output(
        [data['shell'], '-c', '%s --help' % config_status],
        stderr=subprocess.STDOUT).splitlines()
    state = None
    for line in config_status_output:
        if line.startswith('Configuration') and line.endswith(':'):
            if line.endswith('commands:'):
                state = 'commands'
            else:
                state = 'config'
        elif not line.strip():
            state = None
        elif state:
            for f, t in (split_template(couple) for couple in line.split()):
                f = mozpath.join(data['objdir'], f)
                t = mozpath.join(data['srcdir'], t)
                if state == 'commands':
                    command_files.append(f)
                else:
                    config_files.append((f, t))

    return config_files, command_files


def prepare(data_file, srcdir, objdir, shell, args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', type=str)
    parser.add_argument('--host', type=str)
    parser.add_argument('--build', type=str)
    parser.add_argument('--cache-file', type=str)
    # The --srcdir argument is simply ignored. It's a useless autoconf feature
    # that we don't support well anyways. This makes it stripped from `others`
    # and allows to skip setting it when calling the subconfigure (configure
    # will take it from the configure path anyways).
    parser.add_argument('--srcdir', type=str)

    data_file = os.path.join(objdir, data_file)
    previous_args = None
    if os.path.exists(data_file):
        with open(data_file, 'rb') as f:
            data = pickle.load(f)
            previous_args = data['args']

    # Msys likes to break environment variables and command line arguments,
    # so read those from stdin, as they are passed from the configure script
    # when necessary (on windows).
    # However, for some reason, $PATH is not handled like other environment
    # variables, and msys remangles it even when giving it is already a msys
    # $PATH. Fortunately, the mangling/demangling is just find for $PATH, so
    # we can just take the value from the environment. Msys will convert it
    # back properly when calling subconfigure.
    input = sys.stdin.read()
    if input:
        data = {a: b for [a, b] in eval(input)}
        environ = {a: b for a, b in data['env']}
        environ['PATH'] = os.environ['PATH']
        args = data['args']
    else:
        environ = os.environ

    args, others = parser.parse_known_args(args)

    data = {
        'target': args.target,
        'host': args.host,
        'build': args.build,
        'args': others,
        'shell': shell,
        'srcdir': srcdir,
        'env': environ,
    }

    if args.cache_file:
        data['cache-file'] = mozpath.normpath(mozpath.join(os.getcwd(),
            args.cache_file))
    else:
        data['cache-file'] = mozpath.join(objdir, 'config.cache')

    if previous_args is not None:
        data['previous-args'] = previous_args

    try:
        os.makedirs(objdir)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    with open(data_file, 'wb') as f:
        pickle.dump(data, f)


def run(data_file, objdir):
    ret = 0

    with open(os.path.join(objdir, data_file), 'rb') as f:
        data = pickle.load(f)

    data['objdir'] = objdir

    cache_file = data['cache-file']
    cleared_cache = True
    if os.path.exists(cache_file):
        cleared_cache = maybe_clear_cache(data)

    config_files, command_files = get_config_files(data)
    contents = []
    for f, t in config_files:
        contents.append(File(f))

    # AC_CONFIG_COMMANDS actually only registers tags, not file names
    # but most commands are tagged with the file name they create.
    # However, a few don't, or are tagged with a directory name (and their
    # command is just to create that directory)
    for f in command_files:
        if os.path.isfile(f):
            contents.append(File(f))

    # Only run configure if one of the following is true:
    # - config.status doesn't exist
    # - config.status is older than configure
    # - the configure arguments changed
    # - the environment changed in a way that requires a cache clear.
    configure = mozpath.join(data['srcdir'], 'configure')
    config_status_path = mozpath.join(objdir, 'config.status')
    skip_configure = True
    if not os.path.exists(config_status_path):
        skip_configure = False
        config_status = None
    else:
        config_status = File(config_status_path)
        if config_status.mtime < os.path.getmtime(configure) or \
                data.get('previous-args', data['args']) != data['args'] or \
                cleared_cache:
            skip_configure = False

    if not skip_configure:
        command = [data['shell'], configure]
        for kind in ('target', 'build', 'host'):
            if data.get(kind) is not None:
                command += ['--%s=%s' % (kind, data[kind])]
        command += data['args']
        command += ['--cache-file=%s' % cache_file]

        # Pass --no-create to configure so that it doesn't run config.status.
        # We're going to run it ourselves.
        command += ['--no-create']

        print 'configuring in %s' % os.path.relpath(objdir, os.getcwd())
        print 'running %s' % ' '.join(command[:-1])
        sys.stdout.flush()
        ret = subprocess.call(command, cwd=objdir, env=data['env'])

        if ret:
            return ret

        # Leave config.status with a new timestamp if configure is newer than
        # its original mtime.
        if config_status and os.path.getmtime(configure) <= config_status.mtime:
            config_status.update_time()

    # Only run config.status if one of the following is true:
    # - config.status changed or did not exist
    # - one of the templates for config files is newer than the corresponding
    #   config file.
    skip_config_status = True
    if not config_status or config_status.modified:
        # If config.status doesn't exist after configure (because it's not
        # an autoconf configure), skip it.
        if os.path.exists(config_status_path):
            skip_config_status = False
    else:
        # config.status changed or was created, so we need to update the
        # list of config and command files.
        config_files, command_files = get_config_files(data)
        for f, t in config_files:
            if not os.path.exists(t) or \
                    os.path.getmtime(f) < os.path.getmtime(t):
                skip_config_status = False

    if not skip_config_status:
        if skip_configure:
            print 'running config.status in %s' % os.path.relpath(objdir,
                os.getcwd())
            sys.stdout.flush()
        ret = subprocess.call([data['shell'], '-c', './config.status'],
            cwd=objdir, env=data['env'])

        for f in contents:
            f.update_time()

    return ret


CONFIGURE_DATA = 'configure.pkl'

def main(args):
    if args[0] != '--prepare':
        if len(args) != 1:
            raise Exception('Usage: %s relativeobjdir' % __file__)
        return run(CONFIGURE_DATA, args[0])

    topsrcdir = os.path.abspath(args[1])
    subdir = args[2]
    # subdir can be of the form srcdir:objdir
    if ':' in subdir:
        srcdir, subdir = subdir.split(':', 1)
    else:
        srcdir = subdir
    srcdir = os.path.join(topsrcdir, srcdir)
    objdir = os.path.abspath(subdir)

    return prepare(CONFIGURE_DATA, srcdir, objdir, args[3], args[4:])


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
