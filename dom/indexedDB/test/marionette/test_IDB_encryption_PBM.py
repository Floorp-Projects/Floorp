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

        self.testHTML = "dom/indexedDB/basicIDB_PBM.html"
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

    def tearDown(self):
        super(IDBEncryptionPBM, self).tearDown()

        self.marionette.set_pref(INDEXED_DB_PBM_PREF, self.defaultIDBPrefValue)
        self.marionette.set_pref(QM_TESTING_PREF, self.defaultQMPrefValue)

    def test_raw_IDB_data_ondisk(self):
        with self.using_new_window(self.testHTML, private=False) as (
            self.origin,
            self.persistenceType,
        ):
            self.runAndValidate(
                lambda exists: self.assertTrue(
                    exists, "Failed to find expected data on disk"
                )
            )

    def test_ensure_encrypted_IDB_data_ondisk(self):
        with self.using_new_window(self.testHTML, private=True) as (
            self.origin,
            self.persistenceType,
        ):
            self.runAndValidate(
                lambda exists: self.assertFalse(exists, "Data on disk is not encrypted")
            )

    def runAndValidate(self, validator):
        self.marionette.execute_async_script(
            """
                const [idb, store, key, value, resolve] = arguments;
                window.wrappedJSObject.addDataIntoIDB(idb, store, key, value).then(resolve);
            """,
            script_args=(self.IDBName, self.IDBStoreName, "textKey", self.IDBValue),
        )
        self.validateSqlite(validator)

        self.marionette.execute_async_script(
            """
                const [idb, store, key, value, resolve] = arguments;
                const blobValue = new Blob([value], {type:'text/plain'});

                window.wrappedJSObject.addDataIntoIDB(idb, store, key, blobValue).then(resolve);
            """,
            script_args=(self.IDBName, self.IDBStoreName, "blobKey", self.IDBValue),
        )
        self.validateBlob(validator)

    def validateBlob(self, validator):
        self.ensureInvariantHolds(lambda _: self.sqliteWALReleased())
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.getIDBStoragePath(), ".files", False)
            is not None
        )

        idbBlobDir = self.findDirObj(self.getIDBStoragePath(), ".files", False)

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

        validator(foundRawValue)

    def validateSqlite(self, validator):
        self.ensureInvariantHolds(lambda _: self.sqliteWALReleased())
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.getIDBStoragePath(), ".sqlite", True)
            is not None
        )

        sqliteDBFile = self.findDirObj(self.getIDBStoragePath(), ".sqlite", True)

        foundRawValue = False
        with open(sqliteDBFile, "rb") as f_binary:
            foundRawValue = (
                re.search(self.IDBValue.encode("ascii"), f_binary.read()) is not None
            )

        validator(foundRawValue)

    def getIDBStoragePath(self):
        if self.idbStoragePath is not None:
            return self.idbStoragePath

        assert self.origin is not None
        assert self.persistenceType is not None

        self.idbStoragePath = self.getStoragePath(
            self.profilePath, self.origin, self.persistenceType, "idb"
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
