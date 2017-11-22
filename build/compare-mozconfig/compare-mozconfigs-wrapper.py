#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import mozunit
import os
import subprocess
import unittest

from buildconfig import substs

log = logging.getLogger(__name__)


class TestCompareMozconfigs(unittest.TestCase):
    def test_compare_mozconfigs(self):
        topsrcdir = substs['top_srcdir']

        ret = subprocess.call([
            substs['PYTHON'],
            os.path.join(topsrcdir, 'build', 'compare-mozconfig',
                         'compare-mozconfigs.py'),
            topsrcdir
        ])

        self.assertEqual(0, ret)


if __name__ == '__main__':
    mozunit.main()
