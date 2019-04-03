# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
from pytube import YouTube


def download_streams(video_id, output_path="data"):
    yt = YouTube("https://youtube.com/watch?v=%s" % video_id)
    for stream in yt.streams.all():
        fn = "%s-%s-%s.%s" % (video_id, stream.itag, stream.type, stream.subtype)
        stream.download(output_path="data", filename=fn)
        print("%s downloaded" % fn)


if __name__ == "__main__":
    download_streams(sys.argv[-1])
