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


def prepare(srcdir, objdir, args):
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
        'srcdir': srcdir,
        'objdir': objdir,
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

    return data


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


def run(data):
    objdir = data['objdir']
    relobjdir = data['relobjdir'] = os.path.relpath(objdir, os.getcwd())

    cache_file = data['cache-file']
    cleared_cache = True
    if os.path.exists(cache_file):
        cleared_cache = maybe_clear_cache(data)

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
    else:
        config_status_deps = mozpath.join(objdir, 'config_status_deps.in')
        if not os.path.exists(config_status_deps):
            skip_configure = False
        else:
            with open(config_status_deps, 'r') as fh:
                dep_files = fh.read().splitlines() + [configure]
            if (any(not os.path.exists(f) or
                    (os.path.getmtime(config_status_path) < os.path.getmtime(f))
                    for f in dep_files) or
                data.get('previous-args', data['args']) != data['args'] or
                cleared_cache):
                skip_configure = False

    if not skip_configure:
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

    return 0


def main(args):
    topsrcdir = os.path.abspath(args[0])
    srcdir = os.path.join(topsrcdir, 'js/src')
    objdir = os.path.abspath('js/src')

    data = prepare(srcdir, objdir, args[1:])
    return run(data)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
