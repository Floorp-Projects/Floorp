# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase

import os


class ServiceWorkerAtStartupTestCase(MarionetteTestCase):
    def setUp(self):
        super(ServiceWorkerAtStartupTestCase, self).setUp()
        self.install_service_worker()

    def tearDown(self):
        self.marionette.restart(clean=True)
        super(ServiceWorkerAtStartupTestCase, self).tearDown()

    def install_service_worker(self):
        install_url = self.marionette.absolute_url("serviceworker/install_serviceworker.html")
        self.marionette.navigate(install_url)
        Wait(self.marionette).until(
            lambda _: self.is_service_worker_registered,
            message="Wait the service worker to be installed"
        )

    def test_registered_service_worker_after_restart(self):
        # Wait the registered service worker to be stored in the Firefox profile
        # before restarting the instance to prevent intermittent failures
        # (Bug 1665184).
        Wait(self.marionette, timeout=10).until(
            lambda _: self.profile_serviceworker_txt_exists,
            message="Wait service workers to be stored in the profile"
        )

        # Quit and start a new session to simulate a full browser restart
        # (`self.marionette.restart(clean=False, in_app=True)` seems to not
        # be enough to simulate this scenario, because the service workers
        # are staying registered and they are not actually re-registered
        # from the list stored in the profile as this test needs).
        self.marionette.quit(clean=False, in_app=True)
        self.marionette.start_session()

        Wait(self.marionette).until(
            lambda _: self.is_service_worker_registered,
            message="Wait the service worker to be registered after restart"
        )
        self.assertTrue(self.is_service_worker_registered)

    @property
    def profile_serviceworker_txt_exists(self):
        return "serviceworker.txt" in os.listdir(self.marionette.profile_path)

    @property
    def is_service_worker_registered(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script("""
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
            """, script_args=(self.marionette.absolute_url(""),))
