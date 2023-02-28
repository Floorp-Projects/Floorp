# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from service_worker_utils import MarionetteServiceWorkerTestCase


class ServiceWorkersDisabledTestCase(MarionetteServiceWorkerTestCase):
    def setUp(self):
        super(ServiceWorkersDisabledTestCase, self).setUp()
        self.install_service_worker("serviceworker/install_serviceworker.html")

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)
        super(ServiceWorkersDisabledTestCase, self).tearDown()

    def test_service_workers_disabled_at_startup(self):
        # self.marionette.set_pref sets preferences after startup.  Using it
        # here causes intermittent failures.
        self.marionette.instance.profile.set_preferences(
            {
                "dom.serviceWorkers.enabled": False,
            }
        )

        self.marionette.restart()

        self.assertFalse(
            self.is_service_worker_registered,
            "Service worker registration should have been purged",
        )
