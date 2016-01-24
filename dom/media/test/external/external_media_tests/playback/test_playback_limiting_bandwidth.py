# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import BrowserMobProxyTestCaseMixin

from external_media_harness.testcase import NetworkBandwidthTestCase


class TestPlaybackLimitingBandwidth(NetworkBandwidthTestCase,
                                    BrowserMobProxyTestCaseMixin):

    def test_playback_limiting_bandwidth_250(self):
        self.proxy.limits({'downstream_kbps': 250})
        self.run_videos()

    def test_playback_limiting_bandwidth_500(self):
        self.proxy.limits({'downstream_kbps': 500})
        self.run_videos()

    def test_playback_limiting_bandwidth_1000(self):
        self.proxy.limits({'downstream_kbps': 1000})
        self.run_videos()
