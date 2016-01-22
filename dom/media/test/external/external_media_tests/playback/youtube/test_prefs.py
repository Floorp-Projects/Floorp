# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from external_media_harness.testcase import MediaTestCase
from marionette_driver import Wait

from external_media_tests.utils import verbose_until
from external_media_tests.media_utils.youtube_puppeteer import YouTubePuppeteer


class TestMediaSourcePrefs(MediaTestCase):
    def setUp(self):
        MediaTestCase.setUp(self)
        self.test_urls = self.video_urls[:2]
        self.max_timeout = 60

    def tearDown(self):
        MediaTestCase.tearDown(self)

    def test_mse_prefs(self):
        """ 'mediasource' should only be used if MSE prefs are enabled."""
        self.set_mse_enabled_prefs(False)
        self.check_src('http', self.test_urls[0])

        self.set_mse_enabled_prefs(True)
        self.check_src('mediasource', self.test_urls[0])

    def set_mse_enabled_prefs(self, value):
        with self.marionette.using_context('chrome'):
            self.prefs.set_pref('media.mediasource.enabled', value)
            self.prefs.set_pref('media.mediasource.mp4.enabled', value)

    def check_src(self, src_type, url):
        # Why wait to check src until initial ad is done playing?
        # - src attribute in video element is sometimes null during ad playback
        # - many ads still don't use MSE even if main video does
        with self.marionette.using_context('content'):
            youtube = YouTubePuppeteer(self.marionette, url)
            youtube.attempt_ad_skip()
            wait = Wait(youtube,
                        timeout=min(self.max_timeout,
                                    youtube.player_duration * 1.3),
                        interval=1)

            def cond(y):
                return y.video_src.startswith(src_type)

            verbose_until(wait, youtube, cond)
