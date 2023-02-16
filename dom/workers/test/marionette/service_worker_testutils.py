# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class MarionetteServiceWorkerTestCase(MarionetteTestCase):
    def install_service_worker(self, path):
        install_url = self.marionette.absolute_url(path)
        self.marionette.navigate(install_url)
        Wait(self.marionette).until(
            lambda _: self.is_service_worker_registered,
            message="Service worker not successfully installed",
        )

        # Wait for the registered service worker to be stored in the Firefox
        # profile before restarting the instance to prevent intermittent
        # failures (Bug 1665184).
        Wait(self.marionette, timeout=10).until(
            lambda _: self.profile_serviceworker_txt_exists,
            message="Service worker not stored in profile",
        )

        # self.marionette.restart(in_app=True) will restore service workers if
        # we don't navigate away before restarting.
        self.marionette.navigate("about:blank")

    # Using @property helps avoid the case where missing parens at the call site
    # yields an unvarying 'true' value.
    @property
    def profile_serviceworker_txt_exists(self):
        return "serviceworker.txt" in os.listdir(self.marionette.profile_path)

    @property
    def is_service_worker_registered(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
                let swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
                    Ci.nsIServiceWorkerManager
                );
                let ssm = Services.scriptSecurityManager;

                let principal = ssm.createContentPrincipalFromOrigin(arguments[0]);

                let serviceWorkers = swm.getAllRegistrations();
                for (let i = 0; i < serviceWorkers.length; i++) {
                    let sw = serviceWorkers.queryElementAt(
                        i,
                        Ci.nsIServiceWorkerRegistrationInfo
                    );
                    if (sw.principal.origin == principal.origin) {
                        return true;
                    }
                }
                return false;
            """,
                script_args=(self.marionette.absolute_url(""),),
            )
