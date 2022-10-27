# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase

INDEXED_DB_PBM_PREF = "dom.indexedDB.privateBrowsing.enabled"


class IDBEncryptionPBM(MarionetteTestCase):

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

        self.profilePath = self.marionette.instance.profile.profile

        self.defaultPrefValue = self.marionette.get_pref(INDEXED_DB_PBM_PREF)
        self.marionette.set_pref(INDEXED_DB_PBM_PREF, True)

        # Navigate by opening a new private window
        pbmWindowHandle = self.marionette.open(type="window", private=True)["handle"]
        self.marionette.switch_to_window(pbmWindowHandle)
        self.marionette.navigate(
            self.marionette.absolute_url("dom/indexedDB/basicIDB_PBM.html")
        )

        self.idbStoragePath = self.getIDBStoragePath()

    def tearDown(self):
        super(IDBEncryptionPBM, self).setUp()
        self.marionette.set_pref(INDEXED_DB_PBM_PREF, self.defaultPrefValue)

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

        Wait(self.marionette, timeout=60).until(
            lambda _: self.sqliteWALReleased(),
            message="WAL still around even after 60s",
        )

        idbBlobDir = self.findDirObj(self.idbStoragePath, ".files", False)
        if idbBlobDir is None:
            raise Exception(f"unable to find '.files' dir in {self.idbStoragePath}")

        # seems like there's a timing issue here. There are sometimes no blob file
        # even after WAL is released. Allowing some buffer time and ensuring blob file
        # exists before validating it's contents
        idbBlobPath = os.path.join(idbBlobDir, "1")
        Wait(self.marionette, timeout=10).until(
            lambda _: os.path.exists(idbBlobPath),
            message="Blob file does not exist even after waiting 10s",
        )

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

        Wait(self.marionette, timeout=60).until(
            lambda _: self.sqliteWALReleased(),
            message="WAL still around even after 60s",
        )

        sqliteDBFile = self.findDirObj(self.idbStoragePath, ".sqlite", True)
        self.assertIsNotNone(
            sqliteDBFile, f"unable to find .sqlite file in {self.idbStoragePath}"
        )

        foundRawValue = False
        with open(sqliteDBFile, "rb") as f_binary:
            foundRawValue = (
                re.search(self.IDBValue.encode("ascii"), f_binary.read()) is not None
            )

        self.assertFalse(foundRawValue, "sqlite data did not get encrypted")

    def getIDBStoragePath(self):
        origin = (
            self.marionette.absolute_url("")[:-1].replace(":", "+").replace("/", "+")
        )

        # origin directory under storage is suffice'd with '^privateBrowsingId=1' for PBM
        originDir = origin + "^privateBrowsingId=1"

        return os.path.join(self.profilePath, "storage", "default", originDir, "idb")

    def findDirObj(self, path, pattern, isFile):
        for obj in os.scandir(path):
            if obj.path.endswith(pattern) and (obj.is_file() == isFile):
                return obj.path
        return None

    def sqliteWALReleased(self):
        """
        checks if .sqlite-wal has been cleared or not.
        returns False if idbStoragePath does not exist
        """

        if not os.path.exists(self.idbStoragePath):
            return False
        return self.findDirObj(self.idbStoragePath, ".sqlite-wal", True) is None
