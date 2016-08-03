# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from marionette import Marionette
from external_media_harness.testcase import MediaTestCase
from external_media_tests.media_utils.video_puppeteer import VideoPuppeteer


class TestShakaPlayback(MediaTestCase):
    """ Test Widevine playback in shaka-player

    This test takes manifest URLs rather than URLs for pages with videos. These
    manifests are loaded with shaka-player
    """

    def test_video_playback_partial(self):
        """ Plays 60 seconds of the video from the manifest URLs given
        """
        shakaUrl = "http://shaka-player-demo.appspot.com"
        self.prefs.set_pref('media.mediasource.webm.enabled', True)

        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            for manifestUrl in self.video_urls:
                vp = VideoPuppeteer(self.marionette,
                                    shakaUrl,
                                    stall_wait_time=10,
                                    set_duration=60,
                                    video_selector="video#video",
                                    autostart=False)


                manifestInput = self.marionette.find_element("id",
                                                             "manifestUrlInput")
                manifestInput.clear()
                manifestInput.send_keys(manifestUrl)
                loadButton = self.marionette.find_element("id", "loadButton")
                loadButton.click()

                vp.start()
                self.run_playback(vp)
