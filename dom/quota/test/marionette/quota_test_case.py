# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from contextlib import contextmanager

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class QuotaTestCase(MarionetteTestCase):
    def executeAsyncScript(self, script, script_args):
        res = self.marionette.execute_async_script(
            """
            const resolve = arguments[arguments.length - 1];

            class RequestError extends Error {
              constructor(resultCode, resultName) {
                super(`Request failed (code: ${resultCode}, name: ${resultName})`);
                this.name = "RequestError";
                this.resultCode = resultCode;
                this.resultName = resultName;
              }
            }

            async function requestFinished(request) {
              await new Promise(function (resolve) {
                request.callback = function () {
                  resolve();
                };
              });

              if (request.resultCode !== Cr.NS_OK) {
                throw new RequestError(request.resultCode, request.resultName);
              }

              return request.result;
            }
            """
            + script
            + """
            main()
              .then(function(result) {
                resolve(result);
              })
              .catch(function() {
                resolve(null);
              });;
            """,
            script_args=script_args,
            new_sandbox=False,
        )

        assert res is not None
        return res

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
            res = self.executeAsyncScript(
                """
                const [persistenceType, origin] = arguments;

                async function main() {
                  const principal = Services.scriptSecurityManager.
                    createContentPrincipalFromOrigin(origin);

                  const request = Services.qms.getFullOriginMetadata(
                    persistenceType, principal);
                  const metadata = await requestFinished(request);

                  return metadata;
                }
                """,
                script_args=(persistenceType, origin),
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

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            res = self.executeAsyncScript(
                """
                const [origin, persistenceType, client] = arguments;

                async function main() {
                  const principal = Services.scriptSecurityManager.
                    createContentPrincipalFromOrigin(origin);

                  const request = Services.qms.resetStoragesForPrincipal(principal, persistenceType, client);
                  await requestFinished(request);

                  return true;
                }
                """,
                script_args=(origin, persistenceType, client),
            )

            assert res is not None
            return res

    @contextmanager
    def using_new_window(self, path, private=False):
        """
        This helper method is created to ensure that a temporary
        window required inside the test scope is lifetime'd properly
        """

        oldWindow = self.marionette.current_window_handle
        try:
            newWindow = self.marionette.open(type="window", private=private)
            self.marionette.switch_to_window(newWindow["handle"])
            self.marionette.navigate(self.marionette.absolute_url(path))
            origin = self.marionette.absolute_url("")[:-1]
            if private:
                origin += "^privateBrowsingId=1"

            yield (origin, "private" if private else "default")

        finally:
            self.marionette.close()
            self.marionette.switch_to_window(oldWindow)
