# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


class PreloaderWithoutParallelParsingTestCase(MarionetteTestCase):
    def test_restart_with_preloader_cache_without_parallel_parsing(self):
        self.marionette.set_pref("javascript.options.parallel_parsing", False)

        self.marionette.restart()

        # Make sure no crash happens until shutting down.
        self.marionette.quit()
