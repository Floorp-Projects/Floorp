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


CONFIGURE_DATA = 'configure.pkl'


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
            print('Removing %s because of %s value change from:' % (data['cache-file'], precious))
            print('  %s' % (value if value is not None else 'undefined'))
            print('to:')
            print('  %s' % env.get(precious, 'undefined'))
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
    # config.status in js/src never contains the output we try to scan here.
    if data['relobjdir'] == 'js/src':
        return [], []

    config_status = mozpath.join(data['objdir'], 'config.status')
    if not os.path.exists(config_status):
        return [], []

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


def prepare(srcdir, objdir, shell, args):
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

    data_file = os.path.join(objdir, CONFIGURE_DATA)
    previous_args = None
    if os.path.exists(data_file):
        with open(data_file, 'rb') as f:
            data = pickle.load(f)
            previous_args = data['args']

    # Msys likes to break environment variables and command line arguments,
    # so read those from stdin, as they are passed from the configure script
    # when necessary (on windows).
    input = sys.stdin.read()
    if input:
        data = {a: b for [a, b] in eval(input)}
        environ = {a: b for a, b in data['env']}
        # These environment variables as passed from old-configure may contain
        # posix-style paths, which will not be meaningful to the js
        # subconfigure, which runs as a native python process, so use their
        # values from the environment. In the case of autoconf implemented
        # subconfigures, Msys will re-convert them properly.
        for var in ('HOME', 'TERM', 'PATH', 'TMPDIR', 'TMP',
                    'TEMP', 'INCLUDE'):
            if var in environ and var in os.environ:
                environ[var] = os.environ[var]
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


def prefix_lines(text, prefix):
    return ''.join('%s> %s' % (prefix, line) for line in text.splitlines(True))


def execute_and_prefix(*args, **kwargs):
    prefix = kwargs['prefix']
    del kwargs['prefix']
    proc = subprocess.Popen(*args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kwargs)
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        print(prefix_lines(line.rstrip(), prefix))
        sys.stdout.flush()
    return proc.wait()


def run(objdir):
    ret = 0

    with open(os.path.join(objdir, CONFIGURE_DATA), 'rb') as f:
        data = pickle.load(f)

    data['objdir'] = objdir
    relobjdir = data['relobjdir'] = os.path.relpath(objdir, os.getcwd())

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
    # - config.status is older than an input to configure
    # - the configure arguments changed
    # - the environment changed in a way that requires a cache clear.
    configure = mozpath.join(data['srcdir'], 'old-configure')
    config_status_path = mozpath.join(objdir, 'config.status')
    skip_configure = True
    if not os.path.exists(config_status_path):
        skip_configure = False
        config_status = None
    else:
        config_status = File(config_status_path)
        config_status_deps = mozpath.join(objdir, 'config_status_deps.in')
        if not os.path.exists(config_status_deps):
            skip_configure = False
        else:
            with open(config_status_deps, 'r') as fh:
                dep_files = fh.read().splitlines() + [configure]
            if (any(not os.path.exists(f) or (config_status.mtime < os.path.getmtime(f))
                    for f in dep_files) or
                data.get('previous-args', data['args']) != data['args'] or
                cleared_cache):
                skip_configure = False

    if not skip_configure:
        if mozpath.normsep(relobjdir) == 'js/src':
            # Because configure is a shell script calling a python script
            # calling a shell script, on Windows, with msys screwing the
            # environment, we lose the benefits from our own efforts in this
            # script to get past the msys problems. So manually call the python
            # script instead, so that we don't do a native->msys transition
            # here. Then the python configure will still have the right
            # environment when calling the shell configure.
            command = [
                sys.executable,
                os.path.join(os.path.dirname(__file__), '..', 'configure.py'),
                '--enable-project=js',
            ]
            data['env']['OLD_CONFIGURE'] = os.path.join(
                os.path.dirname(configure), 'old-configure')
        else:
            command = [data['shell'], configure]
        for kind in ('target', 'build', 'host'):
            if data.get(kind) is not None:
                command += ['--%s=%s' % (kind, data[kind])]
        command += data['args']
        command += ['--cache-file=%s' % cache_file]

        # Pass --no-create to configure so that it doesn't run config.status.
        # We're going to run it ourselves.
        command += ['--no-create']

        print(prefix_lines('configuring', relobjdir))
        print(prefix_lines('running %s' % ' '.join(command[:-1]), relobjdir))
        sys.stdout.flush()
        returncode = execute_and_prefix(command, cwd=objdir, env=data['env'],
                                        prefix=relobjdir)
        if returncode:
            return returncode

    # Only run config.status if one of the following is true:
    # - config.status changed or did not exist
    # - one of the templates for config files is newer than the corresponding
    #   config file.
    skip_config_status = True
    if mozpath.normsep(relobjdir) == 'js/src':
        # Running config.status in js/src actually does nothing, so we just
        # skip it.
        pass
    elif not config_status or config_status.modified:
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
            print(prefix_lines('running config.status', relobjdir))
            sys.stdout.flush()
        ret = execute_and_prefix([data['shell'], '-c', './config.status'],
                                 cwd=objdir, env=data['env'], prefix=relobjdir)

        for f in contents:
            f.update_time()

    return ret


def subconfigure(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--list', type=str,
                        help='File containing a list of subconfigures to run')
    parser.add_argument('--skip', type=str,
                        help='File containing a list of Subconfigures to skip')
    parser.add_argument('subconfigures', type=str, nargs='*',
                        help='Subconfigures to run if no list file is given')
    args, others = parser.parse_known_args(args)
    subconfigures = args.subconfigures
    if args.list:
        subconfigures.extend(open(args.list, 'rb').read().splitlines())
    if args.skip:
        skips = set(open(args.skip, 'rb').read().splitlines())
        subconfigures = [s for s in subconfigures if s not in skips]

    if not subconfigures:
        return 0

    ret = 0
    for subconfigure in subconfigures:
        returncode = run(subconfigure)
        ret = max(returncode, ret)
        if ret:
            break
    return ret


def main(args):
    if args[0] != '--prepare':
        return subconfigure(args)

    topsrcdir = os.path.abspath(args[1])
    subdir = args[2]
    # subdir can be of the form srcdir:objdir
    if ':' in subdir:
        srcdir, subdir = subdir.split(':', 1)
    else:
        srcdir = subdir
    srcdir = os.path.join(topsrcdir, srcdir)
    objdir = os.path.abspath(subdir)

    return prepare(srcdir, objdir, args[3], args[4:])


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
