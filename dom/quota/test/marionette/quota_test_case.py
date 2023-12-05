# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


class QuotaTestCase(MarionetteTestCase):
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
