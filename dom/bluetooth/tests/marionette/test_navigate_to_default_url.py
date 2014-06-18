# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class testNavigateToDefault(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        # Sets an appropriate timeout for this test.
        # P.S. The timeout of next test wouldn't be affected by this statement.
        self.marionette.timeouts(self.marionette.TIMEOUT_PAGE, 90000)

    def test_navigate_to_default_url(self):
        try:
            self.marionette.navigate("app://system.gaiamobile.org/index.html")
        except:
            self.assertTrue(False, "Can not navigate to system app.")
