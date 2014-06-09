# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class testNavigateToDefault(MarionetteTestCase):
    def test_navigate_to_default_url(self):
        try:
            self.marionette.navigate("app://system.gaiamobile.org/index.html")
        except:
            self.assertTrue(Flase, "Can not navigate to system app.")
