# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import Marionette
from marionette_driver import Wait
from marionette_driver.errors import TimeoutException

from external_media_tests.utils import verbose_until
from external_media_harness.testcase import MediaTestCase
from external_media_tests.media_utils.video_puppeteer import VideoException
from external_media_tests.media_utils.youtube_puppeteer import (YouTubePuppeteer, playback_done,
                                           wait_for_almost_done)


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
                              lambda y: y.video_src.startswith('mediasource'),
                              "Failed to find 'mediasource' in video src url.")
            except TimeoutException as e:
                raise self.failureException(e)

    def test_video_playing_in_one_tab(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                self.logger.info(url)
                youtube = YouTubePuppeteer(self.marionette, url)
                self.logger.info('Expected duration: %s' %
                                 youtube.expected_duration)
                youtube.deactivate_autoplay()

                final_piece = 60
                try:
                    time_left = wait_for_almost_done(youtube,
                                                     final_piece=final_piece)
                except VideoException as e:
                    raise self.failureException(e)
                duration = abs(youtube.expected_duration) + 1
                if duration > 1:
                    self.logger.info('Almost done: %s - %s seconds left.' %
                                     (youtube.movie_id, time_left))
                    if time_left > final_piece:
                        self.marionette.log('time_left greater than '
                                            'final_piece - %s' % time_left,
                                            level='WARNING')
                        self.save_screenshot()
                else:
                    self.marionette.log('Duration close to 0 - %s' % youtube,
                                        level='WARNING')
                    self.save_screenshot()
                try:
                    verbose_until(Wait(youtube,
                                       timeout=max(100, time_left) * 1.3,
                                       interval=1),
                                  youtube,
                                  playback_done)
                except TimeoutException as e:
                    raise self.failureException(e)

    def test_playback_starts(self):
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for url in self.video_urls:
                try:
                    YouTubePuppeteer(self.marionette, url, timeout=60)
                except TimeoutException as e:
                    raise self.failureException(e)
