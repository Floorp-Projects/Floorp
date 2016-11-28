#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import mozunit
import subprocess
import sys
import unittest

from os import path
from buildconfig import substs

log = logging.getLogger(__name__)

def determine_platform():
    platform_mapping = {'WINNT': {'x86_64': 'win64',
                                  'i686': 'win32'},
                        'Darwin': {'x86_64': 'macosx-universal',
                                   'i386':'macosx-universal'},
                        'Linux': {'x86_64': 'linux64',
                                  'i686': 'linux32'}}

    os_type = substs['OS_TARGET']
    cpu_type = substs['TARGET_CPU']
    return platform_mapping.get(os_type, {}).get(cpu_type, None)


class TestCompareMozconfigs(unittest.TestCase):
    def test_compare_mozconfigs(self):
        """ A wrapper script that calls compare-mozconfig.py
        based on the platform that the machine is building for"""
        platform = determine_platform()

        if platform is not None:
            python_exe = substs['PYTHON']
            topsrcdir = substs['top_srcdir']

            # construct paths and args for compare-mozconfig
            browser_dir = path.join(topsrcdir, 'browser')
            script_path = path.join(topsrcdir, 'build/compare-mozconfig/compare-mozconfigs.py')
            whitelist_path = path.join(browser_dir, 'config/mozconfigs/whitelist')
            beta_mozconfig_path = path.join(browser_dir, 'config/mozconfigs', platform, 'beta')
            release_mozconfig_path = path.join(browser_dir, 'config/mozconfigs', platform, 'release')
            nightly_mozconfig_path = path.join(browser_dir, 'config/mozconfigs', platform, 'nightly')

            log.info("Comparing beta against nightly mozconfigs")
            ret_code = subprocess.call([python_exe, script_path, '--whitelist',
                                        whitelist_path, '--no-download',
                                        platform + ',' + beta_mozconfig_path +
                                        ',' + nightly_mozconfig_path])
            self.assertEqual(0, ret_code)


            log.info("Comparing release against nightly mozconfigs")
            ret_code = subprocess.call([python_exe, script_path, '--whitelist',
                                        whitelist_path, '--no-download',
                                        platform + ',' + release_mozconfig_path +
                                        ',' + nightly_mozconfig_path])
            self.assertEqual(0, ret_code)

if __name__ == '__main__':
    mozunit.main()
