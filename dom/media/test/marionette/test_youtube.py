# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import os

sys.path.append(os.path.dirname(__file__))
from yttest.support import VideoStreamTestCase


class YoutubeTest(VideoStreamTestCase):

    # bug 1513511
    def test_stream_4K(self):
        with self.youtube_video("uR0N3DrybGQ", duration=15) as page:
            res = page.run_test()
            self.assertVideoQuality(res)

    def test_stream_480p(self):
        with self.youtube_video("BZP1rYjoBgI", duration=15) as page:
            res = page.run_test()
            self.assertVideoQuality(res)
