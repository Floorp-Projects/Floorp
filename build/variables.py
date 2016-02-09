# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys
from datetime import datetime


def buildid_header(output):
    buildid = os.environ.get('MOZ_BUILD_DATE')
    if buildid and len(buildid) != 14:
        print('Ignoring invalid MOZ_BUILD_DATE: %s' % buildid, file=sys.stderr)
        buildid = None
    if not buildid:
        buildid = datetime.now().strftime('%Y%m%d%H%M%S')
    output.write("#define MOZ_BUILDID %s\n" % buildid)


def main(args):
    if (len(args)):
        func = globals().get(args[0])
        if func:
            return func(sys.stdout, *args[1:])


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
