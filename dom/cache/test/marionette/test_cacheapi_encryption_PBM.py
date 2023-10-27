# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
from pathlib import Path

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase

CACHEAPI_PBM_PREF = "dom.cache.privateBrowsing.enabled"
QM_TESTING_PREF = "dom.quotaManager.testing"


class CacheAPIEncryptionPBM(MarionetteTestCase):

    """
    Bug1856953: Ensure CacheAPI data gets encrypted in Private Browsing Mode.
    We need to ensure data inside both sqlite fields and request/response files
    gets encrypted
    """

    def setUp(self):
        super(CacheAPIEncryptionPBM, self).setUp()

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

        # Navigate by opening a new private window
        pbmWindowHandle = self.marionette.open(type="window", private=True)["handle"]
        self.marionette.switch_to_window(pbmWindowHandle)
        self.marionette.navigate(
            self.marionette.absolute_url("dom/cache/basicCacheAPI_PBM.html")
        )

    def tearDown(self):
        super(CacheAPIEncryptionPBM, self).setUp()

        self.marionette.set_pref(CACHEAPI_PBM_PREF, self.defaultCacheAPIPBMPref)
        self.marionette.set_pref(QM_TESTING_PREF, self.defaultQMPrefValue)

        self.marionette.execute_script("window.wrappedJSObject.releaseCache()")

        # closes the new private window we opened in the setUp and referred by 'pbmWindowHandle'
        self.marionette.close()

    def test_ensure_encrypted_request_response(self):
        self.marionette.execute_async_script(
            """
                const [name, requestStr, responseStr, resolve] = arguments;

                const request = new Request(requestStr);
                const response = new Response(responseStr);
                window.wrappedJSObject.addDataIntoCache(name, request, response)
                                      .then(()=> { resolve(); });
            """,
            script_args=(self.cacheName, self.cacheRequestStr, self.cacheResponseStr),
        )

        self.ensureInvariantHolds(
            lambda _: os.path.exists(self.getCacheAPIStoragePath())
        )

        self.ensureSqliteIsEncrypted()

        # Ensure response bodies have been flushed to the disk
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(self.cacheAPIStoragePath, "morgue", False)
            is not None
        )

        cacheResponseDir = self.findDirObj(self.cacheAPIStoragePath, "morgue", False)

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

        self.assertFalse(foundRawValue, "Response body did not get encrypted")

    def ensureSqliteIsEncrypted(self):
        self.ensureInvariantHolds(
            lambda _: self.findDirObj(
                self.cacheAPIStoragePath, self.cacheDBJournalFileName, True
            )
            is not None
        )
        dbJournalFile = self.findDirObj(
            self.cacheAPIStoragePath, self.cacheDBJournalFileName, True
        )

        self.ensureInvariantHolds(
            lambda _: self.findDirObj(
                self.cacheAPIStoragePath, self.cacheDBFileName, True
            )
            is not None
        )
        dbFile = self.findDirObj(self.cacheAPIStoragePath, self.cacheDBFileName, True)

        # Confirm journal file size is less than 512KB which ensures that checkpoint
        # has not happend yet (dom/cache/DBSchema.cpp::InitializeConnection, kWalAutoCheckpointPages)
        self.assertTrue(
            os.path.getsize(dbJournalFile) < self.dbCheckpointThresholdBytes
        )

        # Before checkpointing, journal file size should be greater than main sqlite db file.
        self.assertTrue(os.path.getsize(dbJournalFile) > os.path.getsize(dbFile))

        self.assertFalse(
            self.cacheRequestStr.encode("ascii") in open(dbJournalFile, "rb").read(),
            "Sqlite journal file did not get encrypted",
        )

        self.assertTrue(
            self.resetStoragesForPrincipal(
                self.marionette.absolute_url("")[:-1] + "^privateBrowsingId=1"
            )
        )

        self.assertFalse(os.path.getsize(dbJournalFile) > os.path.getsize(dbFile))

        self.assertFalse(
            self.cacheRequestStr.encode("ascii") in open(dbFile, "rb").read(),
            "Sqlite main db file did not get encrypted",
        )

    def resetStoragesForPrincipal(self, relatedOrigin):
        # This method is used to force sqlite to write journal file contents to
        # main sqlite database file

        script = (
            """
            const [resolve] = arguments

            function resetStorage() {
                return new Promise((resolve, reject) => {
                    let origin = '%s';
                    let principal = Services.scriptSecurityManager.
                                        createContentPrincipalFromOrigin(origin);

                    let qms = Components.classes["@mozilla.org/dom/quota-manager-service;1"]
                                        .getService(Components.interfaces.nsIQuotaManagerService);

                    let req = qms.resetStoragesForPrincipal(principal, "private", "cache");
                    req.callback = () => {
                        if (req.resultCode != 0) reject([false, req.resultCode]);
                        resolve([true, 0]);
                    };
                });
            }

            return resetStorage()
                .then((result) => resolve(result))
                .catch((error) => resolve(error));
            """
            % relatedOrigin
        )

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_async_script(script)[0]

    def getCacheAPIStoragePath(self):
        if self.cacheAPIStoragePath is not None:
            return self.cacheAPIStoragePath

        fullOriginMetadata = self.getFullOriginMetadata(
            self.marionette.absolute_url("")[:-1] + "^privateBrowsingId=1"
        )

        storageOrigin = fullOriginMetadata["storageOrigin"]
        sanitizedStorageOrigin = storageOrigin.replace(":", "+").replace("/", "+")

        storagePath = os.path.join(
            self.profilePath, "storage", "private", sanitizedStorageOrigin, "cache"
        )

        self.cacheAPIStoragePath = storagePath

        print("cacheAPI origin directory = " + self.cacheAPIStoragePath)
        return self.cacheAPIStoragePath

    def getFullOriginMetadata(self, origin):
        with self.marionette.using_context("chrome"):
            res = self.marionette.execute_async_script(
                """
                    const [url, resolve] = arguments;

                    function getOrigin() {
                        return new Promise((resolve, reject) => {
                            let context = "private"
                            let principal = Services.scriptSecurityManager.
                                createContentPrincipalFromOrigin(url);

                            let qms = Services.qms;
                            let req = qms.getFullOriginMetadata(context, principal);
                            req.callback = () => {
                                console.log("req.resultcode = ", req.resultCode);
                                if (req.resultCode != 0) reject("Error!, resultCode = " + req.resultCode );
                                resolve(req.result);
                            };
                        });
                    }

                    return getOrigin()
                        .then((result) => resolve(result))
                        .catch((error) => resolve("Error!"));
                """,
                script_args=(origin,),
                new_sandbox=False,
            )
            assert res is not None
            return res

    def findDirObj(self, path, pattern, isFile):
        for obj in os.scandir(path):
            if obj.path.endswith(pattern) and (obj.is_file() == isFile):
                return obj.path
        return None

    def ensureInvariantHolds(self, op):
        maxWaitTime = 60
        Wait(self.marionette, timeout=maxWaitTime).until(
            op,
            message=f"operation did not yield success even after waiting {maxWaitTime}s time",
        )
