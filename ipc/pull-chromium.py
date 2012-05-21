# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Pull a specified revision of chromium from SVN.

Usage: python pull-chromium.py <topsrcdir> <chromiumtree> <revision>

You will have to set up a Chromium tree before running this step. See
http://dev.chromium.org/developers/how-tos/get-the-code for details about
doing this efficiently.
"""

import sys, os
from subprocess import check_call
from shutil import rmtree

topsrcdir, chromiumtree, rev = sys.argv[1:]

if not os.path.exists(os.path.join(topsrcdir, 'client.py')):
    print >>sys.stderr, "Incorrect topsrcdir"
    sys.exit(1)

if not os.path.exists(os.path.join(chromiumtree, 'src/DEPS')):
    print >>sys.stderr, "Incorrect chromium directory, missing DEPS"
    sys.exit(1)

check_call(['gclient', 'sync', '--force', '--revision=src@%s' % rev], cwd=chromiumtree)

chromiumsrc = os.path.join(topsrcdir, 'ipc/chromium/src')
os.path.exists(chromiumsrc) and rmtree(chromiumsrc)

def doexport(svnpath):
    localpath = os.path.join(chromiumsrc, svnpath)
    os.makedirs(os.path.dirname(localpath))
    check_call(['svn', 'export', '-r', 'BASE', os.path.join(chromiumtree, 'src', svnpath), localpath])

doexport('base')
doexport('chrome/common')
doexport('build/build_config.h')
doexport('testing/gtest/include')
doexport('third_party/libevent')



