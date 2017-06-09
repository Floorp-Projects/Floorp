# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_driver.errors import TimeoutException
from marionette_harness import Marionette

from external_media_tests.utils import verbose_until
from external_media_harness.testcase import MediaTestCase
from external_media_tests.media_utils.video_puppeteer import VideoException
from external_media_tests.media_utils.youtube_puppeteer import YouTubePuppeteer


class TestBasicYouTubePlayback(MediaTestCase):
    def test_mse_is_enabled_by_default(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            youtube = YouTubePuppeteer(self.marionette, self.video_urls[0],
                                       timeout=60)
            wait = Wait(youtube,
                        timeout=min(300, youtube.expected_duration * 1.3),
                        interval=1)
            try:
                verbose_until(wait, youtube,
                              YouTubePuppeteer.mse_enabled,
                              "Failed to find 'blob' in video src url.")
            except TimeoutException as e:
                raise self.failureException(e)

    def test_video_playing_in_one_tab(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                self.logger.info(url)
                youtube = YouTubePuppeteer(self.marionette, url)
                self.logger.info('Expected duration: {}'
                                 .format(youtube.expected_duration))

                final_piece = 60
                try:
                    time_left = youtube.wait_for_almost_done(
                        final_piece=final_piece)
                except VideoException as e:
                    raise self.failureException(e)
                duration = abs(youtube.expected_duration) + 1
                if duration > 1:
                    self.logger.info('Almost done: {} - {} seconds left.'
                                     .format(url, time_left))
                    if time_left > final_piece:
                        self.logger.warn('time_left greater than '
                                         'final_piece - {}'
                                         .format(time_left))
                        self.save_screenshot()
                else:
                    self.logger.warn('Duration close to 0 - {}'
                                     .format(youtube))
                    self.save_screenshot()
                try:
                    verbose_until(Wait(youtube,
                                       timeout=max(100, time_left) * 1.3,
                                       interval=1),
                                  youtube,
                                  YouTubePuppeteer.playback_done)
                except TimeoutException as e:
                    raise self.failureException(e)

    def test_playback_starts(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                try:
                    YouTubePuppeteer(self.marionette, url, timeout=60)
                except TimeoutException as e:
                    raise self.failureException(e)
