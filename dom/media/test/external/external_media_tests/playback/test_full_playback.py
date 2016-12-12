# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import Marionette

from external_media_harness.testcase import MediaTestCase
from external_media_tests.media_utils.video_puppeteer import VideoPuppeteer


class TestFullPlayback(MediaTestCase):
    """ Test MSE playback in HTML5 video element.

    These tests should pass on any site where a single video element plays
    upon loading and is uninterrupted (by ads, for example). This will play
    the full videos, so it could take a while depending on the videos playing.
    It should be run much less frequently in automated systems.
    """

    def test_video_playback_full(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                video = VideoPuppeteer(self.marionette, url,
                                       stall_wait_time=10)
                self.run_playback(video)
