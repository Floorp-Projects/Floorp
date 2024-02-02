# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import sys
from pathlib import Path

sys.path.append(os.fspath(Path(__file__).parents[3] / "quota/test/marionette"))

from quota_test_case import QuotaTestCase

CACHEAPI_PBM_PREF = "dom.cache.privateBrowsing.enabled"
QM_TESTING_PREF = "dom.quotaManager.testing"


class CacheAPIEncryptionPBM(QuotaTestCase):

    """
    Bug1856953: Ensure CacheAPI data gets encrypted in Private Browsing Mode.
    We need to ensure data inside both sqlite fields and request/response files
    gets encrypted
    """

    def setUp(self):
        super(CacheAPIEncryptionPBM, self).setUp()

        self.testHTML = "dom/cache/basicCacheAPI_PBM.html"
        self.cacheName = "CachePBMTest"
        self.profilePath = self.marionette.instance.profile.profile
        self.cacheAPIStoragePath = None

        self.defaultCacheAPIPBMPref = self.marionette.get_pref(CACHEAPI_PBM_PREF)
        self.marionette.set_pref(CACHEAPI_PBM_PREF, True)

        self.defaultQMPrefValue = self.marionette.get_pref(QM_TESTING_PREF)
        self.marionette.set_pref(QM_TESTING_PREF, True)

        self.cacheRequestStr = "https://example.com/"
        self.cacheResponseStr = "CacheAPIEncryptionPBM"

        self.cacheDBFileName = "caches.sqlite"
        self.cacheDBJournalFileName = "caches.sqlite-wal"

        self.dbCheckpointThresholdBytes = 512 * 1024

    def tearDown(self):
        super(CacheAPIEncryptionPBM, self).setUp()

        self.marionette.set_pref(CACHEAPI_PBM_PREF, self.defaultCacheAPIPBMPref)
        self.marionette.set_pref(QM_TESTING_PREF, self.defaultQMPrefValue)

    def test_request_response_ondisk(self):
        with self.using_new_window(self.testHTML, private=False) as (
            self.origin,
            self.persistenceType,
        ):
            self.runAndValidate(
                lambda exists: self.assertTrue(
                    exists, "Failed to find expected data on disk"
                )
            )

    def test_encrypted_request_response_ondisk(self):
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
                const [name, requestStr, responseStr, resolve] = arguments;

                const request = new Request(requestStr);
                const response = new Response(responseStr);
                window.wrappedJSObject.addDataIntoCache(name, request, response)
                                    .then(resolve);
            """,
            script_args=(
                self.cacheName,
                self.cacheRequestStr,
                self.cacheResponseStr,
            ),
        )

        self.ensureInvariantHolds(
            lambda _: os.path.exists(self.getCacheAPIStoragePath())
        )

        self.validateSqlite(validator)
        self.validateBodyFile(validator)

    def validateBodyFile(self, validator):
        # Ensure response bodies have been flushed to the disk
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.getCacheAPIStoragePath(), "morgue", False)
            is not None
        )

        cacheResponseDir = self.findDirObj(
            self.getCacheAPIStoragePath(), "morgue", False
        )

        self.ensureInvariantHolds(lambda _: any(os.listdir(cacheResponseDir)))

        # Get response bodies directory corresponding to the cache 'self.CacheName' since, there's
        # only one cache object in this origin, it must be the first one.
        cacheResponseBodiesPath = [
            d for d in Path(cacheResponseDir).iterdir() if d.is_dir()
        ][0]

        # Ensure bodies have been transferred to '.final' from '.tmp'
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(cacheResponseBodiesPath, ".final", True)
            is not None
        )
        cacheResponseBodyPath = self.findDirObj(cacheResponseBodiesPath, ".final", True)

        # Since a cache response would get compressed using snappy; and an unencrypted response would
        # contain 'sNaPpY' as a compression header in the response body file. Check to ensure that
        # 'sNaPpy' does not exist if bodies are getting encrypted.
        foundRawValue = False
        with open(cacheResponseBodyPath, "rb") as f_binary:
            foundRawValue = re.search(b"sNaPpY", f_binary.read()) is not None

        validator(foundRawValue)

    def validateSqlite(self, validator):
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(
                self.getCacheAPIStoragePath(), self.cacheDBJournalFileName, True
            )
            is not None
        )
        dbJournalFile = self.findDirObj(
            self.getCacheAPIStoragePath(), self.cacheDBJournalFileName, True
        )

        self.ensureInvariantHolds(
            lambda _: self.findDirObj(
                self.getCacheAPIStoragePath(), self.cacheDBFileName, True
            )
            is not None
        )
        dbFile = self.findDirObj(
            self.getCacheAPIStoragePath(), self.cacheDBFileName, True
        )

        # Confirm journal file size is less than 512KB which ensures that checkpoint
        # has not happend yet (dom/cache/DBSchema.cpp::InitializeConnection, kWalAutoCheckpointPages)
        self.assertTrue(
            os.path.getsize(dbJournalFile) < self.dbCheckpointThresholdBytes
        )

        # Before checkpointing, journal file size should be greater than main sqlite db file.
        self.assertTrue(os.path.getsize(dbJournalFile) > os.path.getsize(dbFile))

        validator(
            self.cacheRequestStr.encode("ascii") in open(dbJournalFile, "rb").read()
        )

        self.assertTrue(
            self.resetStoragesForPrincipal(self.origin, self.persistenceType, "cache")
        )

        self.assertFalse(os.path.getsize(dbJournalFile) > os.path.getsize(dbFile))

        validator(self.cacheRequestStr.encode("ascii") in open(dbFile, "rb").read())

    def getCacheAPIStoragePath(self):
        if self.cacheAPIStoragePath is not None:
            return self.cacheAPIStoragePath

        assert self.origin is not None
        assert self.persistenceType is not None

        self.cacheAPIStoragePath = self.getStoragePath(
            self.profilePath, self.origin, self.persistenceType, "cache"
        )

        print("cacheAPI origin directory = " + self.cacheAPIStoragePath)
        return self.cacheAPIStoragePath
