# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from pathlib import Path

sys.path.append(os.fspath(Path(__file__).parents[0]))
from quota_test_case import QuotaTestCase

QM_TESTING_PREF = "dom.quotaManager.testing"
AUTOSTART_PBM_PREF = "browser.privatebrowsing.autostart"

"""
    This test ensures that private repository gets properly purged in all below scenarios:
        1. at PBM session end
        2. on firefox start when last session was abnormally terminated (crash)
        3. on firefox shutdown (when PBM autostart is enabled i.e. browser.privatebrowsing.autostart set to true)
"""


class PrivateRepositoryCleanup(QuotaTestCase):
    def setUp(self, autostartPBM=False):
        super(PrivateRepositoryCleanup, self).setUp()

        self.marionette.set_pref(AUTOSTART_PBM_PREF, autostartPBM)
        self.marionette.set_pref(QM_TESTING_PREF, True)

        self.marionette.restart(in_app=True)

        assert self.initStorage()
        assert self.initTemporaryStorage()

    def tearDown(self):
        self.marionette.clear_pref(AUTOSTART_PBM_PREF)
        self.marionette.clear_pref(QM_TESTING_PREF)

        self.marionette.restart(in_app=True)
        super(PrivateRepositoryCleanup, self).tearDown()

    def doStorageWork(self):
        origin = self.marionette.absolute_url("")[:-1] + "^privateBrowsingId=1"
        assert self.initTemporaryOrigin("private", origin)

        self.ensureInvariantHolds(lambda _: os.path.exists(self.getPrivateRepository()))

    def verifyCleanup(self):
        self.ensureInvariantHolds(
            lambda _: not os.path.exists(self.getPrivateRepository())
        )

    def getPrivateRepository(self):
        return self.getRepositoryPath("private")


class PBM(PrivateRepositoryCleanup):
    def test_ensure_cleanup(self):
        with self.using_new_window("", private=True):
            self.doStorageWork()

        # private window must have been close by now
        self.verifyCleanup()

    def test_ensure_cleanup_after_crash(self):
        with self.using_new_window("", private=True, skipCleanup=True):
            self.doStorageWork()

        # verify cleanup was performed after crashing and restarting
        # firefox with in_app=False
        self.marionette.restart(in_app=False)
        self.verifyCleanup()


class PBMAutoStart(PrivateRepositoryCleanup):
    def setUp(self):
        super(PBMAutoStart, self).setUp(True)

    def test_ensure_cleanup(self):
        self.doStorageWork()

        # verify cleanup was performed at the shutdown
        self.marionette.quit(in_app=True)
        self.verifyCleanup()

        self.marionette.start_session()

    def test_ensure_cleanup_after_crash(self):
        self.doStorageWork()

        # verify cleanup was performed after crashing and restarting
        # firefox with in_app=False
        self.marionette.restart(in_app=False)
        self.verifyCleanup()
