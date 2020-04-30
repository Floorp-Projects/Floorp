# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import os
import errno


def mtime(path):
    try:
        return os.stat(path).st_mtime
    except OSError as e:
        if e.errno == errno.ENOENT:
            return -1
        raise


def rebuild_check(args):
    target = args[0]
    deps = args[1:]
    t = mtime(target)
    if t < 0:
        print(target)
        return

    newer = []
    removed = []
    for dep in deps:
        deptime = mtime(dep)
        if deptime < 0:
            removed.append(dep)
        elif deptime > t:
            newer.append(dep)

    def format_filelist(filelist):
        if not filelist:
            return None

        limit = 5
        length = len(filelist)
        if length < limit:
            return ', '.join(filelist)

        truncated = filelist[:limit]
        remaining = length - limit

        return '%s (and %d other files)' % (', '.join(truncated), remaining)

    newer = format_filelist(newer)
    removed = format_filelist(removed)

    if newer and removed:
        print('Rebuilding %s because %s changed and %s was removed' % (
            target, newer, removed))
    elif newer:
        print('Rebuilding %s because %s changed' % (target, newer))
    elif removed:
        print('Rebuilding %s because %s was removed' % (
            target, removed))
    else:
        print('Rebuilding %s for an unknown reason' % target)


if __name__ == '__main__':
    import sys
    rebuild_check(sys.argv[1:])
