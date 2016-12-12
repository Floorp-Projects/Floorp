# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import BrowserMobProxyTestCaseMixin

from external_media_harness.testcase import NetworkBandwidthTestCase


class TestUltraLowBandwidth(NetworkBandwidthTestCase,
                                    BrowserMobProxyTestCaseMixin):

    def test_playback_limiting_bandwidth_160(self):
        self.proxy.limits({'downstream_kbps': 160})
        self.run_videos(timeout=120)
