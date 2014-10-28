#! /usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Prints the given arguments in sorted order with duplicates removed.'''

import sys

print(' '.join(sorted(set(sys.argv[1:]))))
