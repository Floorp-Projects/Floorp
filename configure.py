# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import codecs
import glob
import itertools
import json
import os
import subprocess
import sys
import re
import types


base_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(base_dir, 'python', 'which'))
sys.path.append(os.path.join(base_dir, 'python', 'mozbuild'))
from which import which, WhichError
from mozbuild.mozconfig import MozconfigLoader


# If feel dirty replicating this from python/mozbuild/mozbuild/mozconfig.py,
# but the end goal being that the configure script would go away...
shell = 'sh'
if 'MOZILLABUILD' in os.environ:
    shell = os.environ['MOZILLABUILD'] + '/msys/bin/sh'
if sys.platform == 'win32':
    shell = shell + '.exe'


def is_absolute_or_relative(path):
    if os.altsep and os.altsep in path:
        return True
    return os.sep in path


def find_program(file):
    if is_absolute_or_relative(file):
        return os.path.abspath(file) if os.path.isfile(file) else None
    try:
        return which(file)
    except WhichError:
        return None


def autoconf_refresh(configure):
    if os.path.exists(configure):
        mtime = os.path.getmtime(configure)
        aclocal = os.path.join(base_dir, 'build', 'autoconf', '*.m4')
        for input in itertools.chain(
            (configure + '.in',
             os.path.join(os.path.dirname(configure), 'aclocal.m4')),
            glob.iglob(aclocal),
        ):
            if os.path.getmtime(input) > mtime:
                break
        else:
            return

    mozconfig_autoconf = None
    configure_dir = os.path.dirname(configure)
    # Don't read the mozconfig for the js configure (yay backwards
    # compatibility)
    if not configure_dir.replace(os.sep, '/').endswith('/js/src'):
        loader = MozconfigLoader(os.path.dirname(configure))
        project = os.environ.get('MOZ_CURRENT_PROJECT')
        mozconfig = loader.find_mozconfig(env=os.environ)
        mozconfig = loader.read_mozconfig(mozconfig, moz_build_app=project)
        make_extra = mozconfig['make_extra']
        if make_extra:
            for assignment in make_extra:
                m = re.match('(?:export\s+)?AUTOCONF\s*:?=\s*(.+)$',
                             assignment)
                if m:
                    mozconfig_autoconf = m.group(1)

    for ac in (mozconfig_autoconf, os.environ.get('AUTOCONF'), 'autoconf-2.13',
               'autoconf2.13', 'autoconf213'):
        if ac:
            autoconf = find_program(ac)
            if autoconf:
                break
    else:
        fink = find_program('fink')
        if fink:
            autoconf = os.path.normpath(os.path.join(
                fink, '..', '..', 'lib', 'autoconf2.13', 'bin', 'autoconf'))

    if not autoconf:
        raise RuntimeError('Could not find autoconf 2.13')

    # Add or adjust AUTOCONF for subprocesses, especially the js/src configure
    os.environ['AUTOCONF'] = autoconf

    print('Refreshing %s with %s' % (configure, autoconf), file=sys.stderr)

    with open(configure, 'wb') as fh:
        subprocess.check_call([
            shell, autoconf, '--localdir=%s' % os.path.dirname(configure),
            configure + '.in'], stdout=fh)


def main(args):
    old_configure = os.environ.get('OLD_CONFIGURE')

    if not old_configure:
        raise Exception('The OLD_CONFIGURE environment variable must be set')

    # We need to replace backslashes with forward slashes on Windows because
    # this path actually ends up literally as $0, which breaks autoconf's
    # detection of the source directory.
    old_configure = os.path.abspath(old_configure).replace(os.sep, '/')

    try:
        autoconf_refresh(old_configure)
    except RuntimeError as e:
        print(e.message, file=sys.stderr)
        return 1

    ret = subprocess.call([shell, old_configure] + args)
    # We don't want to create and run config.status if --help was one of the
    # command line arguments.
    if ret or '--help' in args:
        return ret

    raw_config = {}
    encoding = 'mbcs' if sys.platform == 'win32' else 'utf-8'
    with codecs.open('config.data', 'r', encoding) as fh:
        code = compile(fh.read(), 'config.data', 'exec')
        # Every variation of the exec() function I tried led to:
        # SyntaxError: unqualified exec is not allowed in function 'main' it
        # contains a nested function with free variables
        exec code in raw_config
    # If the code execution above fails, we want to keep the file around for
    # debugging.
    os.remove('config.data')

    # Sanitize config data
    config = {}
    substs = config['substs'] = {
        k[1:-1]: v[1:-1] if isinstance(v, types.StringTypes) else v
        for k, v in raw_config['substs']
    }
    config['defines'] = {
        k[1:-1]: v[1:-1]
        for k, v in raw_config['defines']
    }
    config['non_global_defines'] = raw_config['non_global_defines']
    config['topsrcdir'] = base_dir
    config['topobjdir'] = os.path.abspath(os.getcwd())

    # Create config.status. Eventually, we'll want to just do the work it does
    # here, when we're able to skip configure tests/use cached results/not rely
    # on autoconf.
    print("Creating config.status", file=sys.stderr)
    with codecs.open('config.status', 'w', encoding) as fh:
        fh.write('#!%s\n' % config['substs']['PYTHON'])
        fh.write('# coding=%s\n' % encoding)
        for k, v in config.iteritems():
            fh.write('%s = ' % k)
            json.dump(v, fh, sort_keys=True, indent=4, ensure_ascii=False)
            fh.write('\n')
        fh.write("__all__ = ['topobjdir', 'topsrcdir', 'defines', "
                 "'non_global_defines', 'substs']")

        if not substs.get('BUILDING_JS') or substs.get('JS_STANDALONE'):
            fh.write('''
if __name__ == '__main__':
    args = dict([(name, globals()[name]) for name in __all__])
    from mozbuild.config_status import config_status
    config_status(**args)
''')

    # Other things than us are going to run this file, so we need to give it
    # executable permissions.
    os.chmod('config.status', 0755)
    if not substs.get('BUILDING_JS') or substs.get('JS_STANDALONE'):
        if not substs.get('JS_STANDALONE'):
            os.environ['WRITE_MOZINFO'] = '1'
        # Until we have access to the virtualenv from this script, execute
        # config.status externally, with the virtualenv python.
        return subprocess.call([config['substs']['PYTHON'], 'config.status'])
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
