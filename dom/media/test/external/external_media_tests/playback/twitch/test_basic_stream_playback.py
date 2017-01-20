# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import TimeoutException
from marionette_harness import Marionette

from external_media_harness.testcase import MediaTestCase
from external_media_tests.media_utils.twitch_puppeteer import TwitchPuppeteer


class TestBasicStreamPlayback(MediaTestCase):
    def test_video_playback_partial(self):
        """
        Test to make sure that playback of 60 seconds works for each video.
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                stream = TwitchPuppeteer(self.marionette, url,
                                         stall_wait_time=10,
                                         set_duration=60)
                self.run_playback(stream)

    def test_playback_starts(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                try:
                    TwitchPuppeteer(self.marionette, url, timeout=60)
                except TimeoutException as e:
                    raise self.failureException(e)
