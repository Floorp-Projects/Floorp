# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import sys
from pathlib import Path

sys.path.append(os.fspath(Path(__file__).parents[3] / "quota/test/marionette"))

from quota_test_case import QuotaTestCase

INDEXED_DB_PBM_PREF = "dom.indexedDB.privateBrowsing.enabled"
QM_TESTING_PREF = "dom.quotaManager.testing"


class IDBEncryptionPBM(QuotaTestCase):

    """
    Bug1784966: Ensure IDB data gets encrypted in Private Browsing Mode.
    We need to ensure data inside both sqlite fields and blob files under
    *.sqllite gets encrypted.
    """

    def setUp(self):
        super(IDBEncryptionPBM, self).setUp()

        self.IDBName = "IDBTest"
        self.IDBStoreName = "IDBTestStore"
        self.IDBVersion = 1
        self.IDBValue = "test_IDB_Encryption_PBM"
        self.idbStoragePath = None

        self.profilePath = self.marionette.instance.profile.profile

        self.defaultIDBPrefValue = self.marionette.get_pref(INDEXED_DB_PBM_PREF)
        self.marionette.set_pref(INDEXED_DB_PBM_PREF, True)

        self.defaultQMPrefValue = self.marionette.get_pref(QM_TESTING_PREF)
        self.marionette.set_pref(QM_TESTING_PREF, True)

        # Navigate by opening a new private window
        pbmWindowHandle = self.marionette.open(type="window", private=True)["handle"]
        self.marionette.switch_to_window(pbmWindowHandle)
        self.marionette.navigate(
            self.marionette.absolute_url("dom/indexedDB/basicIDB_PBM.html")
        )

        self.origin = self.marionette.absolute_url("")[:-1] + "^privateBrowsingId=1"

    def tearDown(self):
        super(IDBEncryptionPBM, self).setUp()

        self.marionette.set_pref(INDEXED_DB_PBM_PREF, self.defaultIDBPrefValue)
        self.marionette.set_pref(QM_TESTING_PREF, self.defaultQMPrefValue)

        # closes the new private window we opened in the setUp and referred by 'pbmWindowHandle'
        self.marionette.close()

    def test_ensure_encrypted_blob(self):
        self.marionette.execute_script(
            """
                const [idb, store, key, value] = arguments;
                const blobValue = new Blob([value], {type:'text/plain'});

                window.wrappedJSObject.addDataIntoIDB(idb, store, key, blobValue);
            """,
            script_args=(self.IDBName, self.IDBStoreName, "blobKey", self.IDBValue),
        )

        self.ensureInvariantHolds(lambda _: self.sqliteWALReleased())
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.idbStoragePath, ".files", False) is not None
        )

        idbBlobDir = self.findDirObj(self.idbStoragePath, ".files", False)

        # seems like there's a timing issue here. There are sometimes no blob file
        # even after WAL is released. Allowing some buffer time and ensuring blob file
        # exists before validating it's contents
        idbBlobPath = os.path.join(idbBlobDir, "1")
        self.ensureInvariantHolds(lambda _: os.path.exists(idbBlobPath))

        foundRawValue = False
        with open(idbBlobPath, "rb") as f_binary:
            foundRawValue = (
                re.search(self.IDBValue.encode("ascii"), f_binary.read()) is not None
            )

        self.assertFalse(foundRawValue, "Blob file did not get encrypted")

    def test_ensure_encrpted_sqlite_data(self):
        self.marionette.execute_script(
            """
                const [idb, store, key, value] = arguments;
                window.wrappedJSObject.addDataIntoIDB(idb, store, key, value);
            """,
            script_args=(self.IDBName, self.IDBStoreName, "textKey", self.IDBValue),
        )

        self.ensureInvariantHolds(lambda _: self.sqliteWALReleased())
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.idbStoragePath, ".sqlite", True) is not None
        )

        sqliteDBFile = self.findDirObj(self.idbStoragePath, ".sqlite", True)

        foundRawValue = False
        with open(sqliteDBFile, "rb") as f_binary:
            foundRawValue = (
                re.search(self.IDBValue.encode("ascii"), f_binary.read()) is not None
            )

        self.assertFalse(foundRawValue, "sqlite data did not get encrypted")

    def getIDBStoragePath(self):
        if self.idbStoragePath is not None:
            return self.idbStoragePath

        self.idbStoragePath = self.getStoragePath(
            self.profilePath, self.origin, "private", "idb"
        )

        print("idb origin directory = " + self.idbStoragePath)
        return self.idbStoragePath

    def sqliteWALReleased(self):
        """
        checks if .sqlite-wal has been cleared or not.
        returns False if idbStoragePath does not exist
        """
        if not os.path.exists(self.getIDBStoragePath()):
            return False
        walPath = self.findDirObj(self.idbStoragePath, ".sqlite-wal", True)
        return walPath is None or os.stat(walPath).st_size == 0
