# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from marionette_driver import Wait
from service_worker_utils import MarionetteServiceWorkerTestCase


class ServiceWorkerAtStartupTestCase(MarionetteServiceWorkerTestCase):
    def setUp(self):
        super(ServiceWorkerAtStartupTestCase, self).setUp()
        self.install_service_worker("serviceworker/install_serviceworker.html")

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)
        super(ServiceWorkerAtStartupTestCase, self).tearDown()

    def test_registered_service_worker_after_restart(self):
        self.marionette.restart()

        Wait(self.marionette).until(
            lambda _: self.is_service_worker_registered,
            message="Service worker not registered after restart",
        )
        self.assertTrue(self.is_service_worker_registered)
