# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import glob
import itertools
import os
import subprocess
import sys

base_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(base_dir, 'python', 'which'))
from which import which, WhichError


# If feel dirty replicating this from python/mozbuild/mozbuild/mozconfig.py,
# but the end goal being that the configure script would go away...
shell = 'sh'
if 'MOZILLABUILD' in os.environ:
    shell = os.environ['MOZILLABUILD'] + '/msys/bin/sh'
if sys.platform == 'win32':
    shell = shell + '.exe'


def find_program(file):
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

    for ac in ('autoconf-2.13', 'autoconf2.13', 'autoconf213'):
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

    print('Refreshing %s' % configure, file=sys.stderr)

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
