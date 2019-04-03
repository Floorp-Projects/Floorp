# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import os

sys.path.append(os.path.dirname(__file__))
from yttest.support import VideoStreamTestCase


class YoutubeTest(VideoStreamTestCase):

    # bug 1513511
    def test_stream_30_seconds(self):
        # XXX use the VP9 video we will settle on.
        with self.youtube_video("BZP1rYjoBgI") as page:
            res = page.run_test()
            self.assertTrue(res is not None, "We did not get back the results")
            self.assertLess(res["droppedVideoFrames"], res["totalVideoFrames"] * 0.04)
            # extracting in/out from the debugInfo
            video_state = res["debugInfo"][7]
            video_in = int(video_state.split(" ")[10].split("=")[-1])
            video_out = int(video_state.split(" ")[11].split("=")[-1])
            # what's the ratio ? we want 99%+
            if video_out == video_in:
                return
            in_out_ratio = float(video_out) / float(video_in) * 100
            self.assertMore(in_out_ratio, 99.0)
