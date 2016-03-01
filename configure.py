# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import glob
import itertools
import os
import subprocess
import sys
import re

base_dir = os.path.dirname(__file__)
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

    return subprocess.call([shell, old_configure] + args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
