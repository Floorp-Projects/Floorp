# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette import BrowserMobProxyTestCaseMixin, MarionetteTestCase
from marionette_driver import Wait
from marionette_driver.errors import TimeoutException
from marionette.marionette_test import SkipTest

from firefox_puppeteer.testcases import BaseFirefoxTestCase
from external_media_tests.utils import (timestamp_now, verbose_until)
from external_media_tests.media_utils.video_puppeteer import (playback_done, playback_started,
                                         VideoException, VideoPuppeteer as VP)


class MediaTestCase(BaseFirefoxTestCase, MarionetteTestCase):

    """
    Necessary methods for MSE playback

    :param video_urls: Urls you are going to play as part of the tests.
    """

    def __init__(self, *args, **kwargs):
        self.video_urls = kwargs.pop('video_urls', False)
        super(MediaTestCase, self).__init__(*args, **kwargs)

    def save_screenshot(self):
        """
        Make a screenshot of the window that is currently playing the video
        element.
        """
        screenshot_dir = os.path.join(self.marionette.instance.workspace or '',
                                      'screenshots')
        filename = ''.join([self.id().replace(' ', '-'),
                            '_',
                            str(timestamp_now()),
                            '.png'])
        path = os.path.join(screenshot_dir, filename)
        if not os.path.exists(screenshot_dir):
            os.makedirs(screenshot_dir)
        with self.marionette.using_context('content'):
            img_data = self.marionette.screenshot()
        with open(path, 'wb') as f:
            f.write(img_data.decode('base64'))
        self.marionette.log('Screenshot saved in %s' % os.path.abspath(path))

    def log_video_debug_lines(self):
        """
        Log the debugging information that Firefox provides for video elements.
        """
        with self.marionette.using_context('chrome'):
            debug_lines = self.marionette.execute_script(VP._debug_script)
            if debug_lines:
                self.marionette.log('\n'.join(debug_lines))

    def run_playback(self, video):
        """
        Play the video all of the way through, or for the requested duration,
        whichever comes first. Raises if the video stalls for too long.

        :param video: VideoPuppeteer instance to play.
        """
        with self.marionette.using_context('content'):
            self.logger.info(video.test_url)
            try:
                verbose_until(Wait(video, interval=video.interval,
                                   timeout=video.expected_duration * 1.3 +
                                   video.stall_wait_time),
                              video, playback_done)
            except VideoException as e:
                raise self.failureException(e)

    def check_playback_starts(self, video):
        """
        Check to see if a given video will start. Raises if the video does not
        start.

        :param video: VideoPuppeteer instance to play.
        """
        with self.marionette.using_context('content'):
            self.logger.info(video.test_url)
            try:
                verbose_until(Wait(video, timeout=video.timeout),
                              video, playback_started)
            except TimeoutException as e:
                raise self.failureException(e)

    def skipTest(self, reason):
        """
        Skip this test.

        Skip with marionette.marionette_test import SkipTest so that it
        gets recognized a skip in marionette.marionette_test.CommonTestCase.run
        """
        raise SkipTest(reason)


class NetworkBandwidthTestCase(MediaTestCase):
    """
    Test MSE playback while network bandwidth is limited. Uses browsermobproxy
    (https://bmp.lightbody.net/). Please see
    https://developer.mozilla.org/en-US/docs/Mozilla/QA/external-media-tests
    for more information on setting up browsermob_proxy.
    """

    def __init__(self, *args, **kwargs):
        super(NetworkBandwidthTestCase, self).__init__(*args, **kwargs)
        BrowserMobProxyTestCaseMixin.__init__(self, *args, **kwargs)
        self.proxy = None

    def setUp(self):
        super(NetworkBandwidthTestCase, self).setUp()
        BrowserMobProxyTestCaseMixin.setUp(self)
        self.proxy = self.create_browsermob_proxy()

    def tearDown(self):
        super(NetworkBandwidthTestCase, self).tearDown()
        BrowserMobProxyTestCaseMixin.tearDown(self)
        self.proxy = None

    def run_videos(self):
        """
        Run each of the videos in the video list. Raises if something goes
        wrong in playback.
        """
        with self.marionette.using_context('content'):
            for url in self.video_urls:
                video = VP(self.marionette, url,
                                       stall_wait_time=60,
                                       set_duration=60)
                self.run_playback(video)


class VideoPlaybackTestsMixin(object):

    """ Test MSE playback in HTML5 video element.

    These tests should pass on any site where a single video element plays
    upon loading and is uninterrupted (by ads, for example).

    This tests both starting videos and performing partial playback at one
    minute each, and is the test that should be run frequently in automation.
    """

    def test_playback_starts(self):
        """
        Test to make sure that playback of the video element starts for each
        video.
        """
        with self.marionette.using_context('content'):
            for url in self.video_urls:
                try:
                    video = VP(self.marionette, url, timeout=60)
                    # Second playback_started check in case video._start_time
                    # is not 0
                    self.check_playback_starts(video)
                    video.pause()
                    src = video.video_src
                    if not src.startswith('mediasource'):
                        self.marionette.log('video is not '
                                            'mediasource: %s' % src,
                                            level='WARNING')
                except TimeoutException as e:
                    raise self.failureException(e)

    def test_video_playback_partial(self):
        """
        Test to make sure that playback of 60 seconds works for each video.
        """
        with self.marionette.using_context('content'):
            for url in self.video_urls:
                video = VP(self.marionette, url,
                           stall_wait_time=10,
                           set_duration=60)
                self.run_playback(video)
