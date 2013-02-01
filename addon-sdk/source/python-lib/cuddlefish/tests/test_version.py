# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
import shutil

from cuddlefish._version import get_versions

class Version(unittest.TestCase):
    def get_basedir(self):
        return os.path.join(".test_tmp", self.id())
    def make_basedir(self):
        basedir = self.get_basedir()
        if os.path.isdir(basedir):
            here = os.path.abspath(os.getcwd())
            assert os.path.abspath(basedir).startswith(here) # safety
            shutil.rmtree(basedir)
        os.makedirs(basedir)
        return basedir

    def test_current_version(self):
        # the SDK should be able to determine its own version. We don't care
        # what it is, merely that it can be computed.
        version = get_versions()["version"]
        self.failUnless(isinstance(version, str), (version, type(version)))
        self.failUnless(len(version) > 0, version)
