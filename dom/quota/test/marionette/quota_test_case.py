# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class QuotaTestCase(MarionetteTestCase):
    def ensureInvariantHolds(self, op):
        maxWaitTime = 60
        Wait(self.marionette, timeout=maxWaitTime).until(
            op,
            message=f"operation did not yield success even after waiting {maxWaitTime}s time",
        )

    def findDirObj(self, path, pattern, isFile):
        for obj in os.scandir(path):
            if obj.path.endswith(pattern) and (obj.is_file() == isFile):
                return obj.path
        return None

    def getFullOriginMetadata(self, persistenceType, origin):
        with self.marionette.using_context("chrome"):
            res = self.marionette.execute_async_script(
                """
                    const [persistenceType, origin, resolve] = arguments;

                    const principal = Services.scriptSecurityManager.
                        createContentPrincipalFromOrigin(origin);

                    const request = Services.qms.getFullOriginMetadata(
                        persistenceType, principal);

                    request.callback = function() {
                      if (request.resultCode != Cr.NS_OK) {
                        resolve(null);
                      } else {
                        resolve(request.result);
                      }
                    }
                """,
                script_args=(persistenceType, origin),
                new_sandbox=False,
            )

            assert res is not None
            return res

    def getStoragePath(self, profilePath, origin, persistenceType, client):
        fullOriginMetadata = self.getFullOriginMetadata(persistenceType, origin)

        storageOrigin = fullOriginMetadata["storageOrigin"]
        sanitizedStorageOrigin = storageOrigin.replace(":", "+").replace("/", "+")

        return os.path.join(
            profilePath, "storage", persistenceType, sanitizedStorageOrigin, client
        )

    def resetStoragesForPrincipal(self, origin, persistenceType, client):
        # This method is used to force sqlite to write journal file contents to
        # main sqlite database file

        script = """
            const [resolve] = arguments

            let origin = '%s';
            let persistenceType = '%s';
            let client = '%s';
            let principal = Services.scriptSecurityManager.
                                createContentPrincipalFromOrigin(origin);

            let req = Services.qms.resetStoragesForPrincipal(principal, persistenceType, client);
            req.callback = () => {
                if (req.resultCode == Cr.NS_OK) {
                    resolve(true);
                } else {
                    resolve(false);
                }
            }
            """ % (
            origin,
            persistenceType,
            client,
        )

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_async_script(script)
