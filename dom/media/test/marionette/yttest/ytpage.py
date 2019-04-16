# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Drives the browser during the playback test.
"""
import contextlib
import os
import time
import json
import re

from marionette_driver.by import By


here = os.path.dirname(__file__)
js = os.path.join(here, "until_end_test.js")
with open(js) as f:
    UNTIL_END_TEST = f.read()

js = os.path.join(here, "duration_test.js")
with open(js) as f:
    DURATION_TEST = f.read()

JS_MACROS = {"video_playback_quality": "", "debug_info": "", "force_hd": ""}
for script in JS_MACROS:
    js = os.path.join(here, "%s.js" % script)
    with open(js) as f:
        JS_MACROS[script] = f.read()

SPLIT_FIELD = (
    "Audio State",
    "Audio Track Buffer Details",
    "AudioSink",
    "MDSM",
    "Video State",
    "Video Track Buffer Details",
    "Dumping Audio Track",
    "Dumping Video Track",
    "MediaDecoder",
    "VideoSink",
    "Dropped Frames"
)


class YoutubePage:
    def __init__(self, video_id, marionette, **options):
        self.video_id = video_id
        self.marionette = marionette
        self.url = "https://www.youtube.com/watch?v=%s" % self.video_id
        self.started = False
        self.capabilities = {
            # We're not using upstream cert sniffing, let's make sure
            # the browser accepts mitmproxy ones for all requests
            # even if they are incorrect.
            "acceptInsecureCerts": True
        }
        self.options = options
        if options.get("proxy", True):
            self.capabilities["proxy"] = {
                "proxyType": "manual",
                "httpProxy": "localhost:8080",
                "sslProxy": "localhost:8080",
                "noProxy": ["localhost"],
            }

    def start_video(self):
        self.marionette.start_session(self.capabilities)
        self.marionette.timeout.script = 600
        self.marionette.navigate(self.url)
        self.started = True

    def run_test(self):
        self.start_video()
        # If we don't pause here for just a bit the media events
        # are not intercepted.
        time.sleep(5)
        body = self.marionette.find_element(By.TAG_NAME, "html")
        body.click()
        options = dict(JS_MACROS)
        options.update(self.options)
        if "duration" in options:
            script = DURATION_TEST % options
        else:
            script = UNTIL_END_TEST % options
        res = self.execute_async_script(script)
        if res is None:
            return res
        res = self._parse_res(res)
        self._dump_res(res)
        return res

    def execute_async_script(self, script, context=None):
        if context is None:
            context = self.marionette.CONTEXT_CONTENT
        with self.marionette.using_context(context):
            return self.marionette.execute_async_script(script, sandbox="system")

    def _parse_res(self, res):
        debug_info = {}
        # The parsing won't be necessary once we have bug 1542674
        for key, value in res["mozRequestDebugInfo"].items():
            key, value = key.strip(), value.strip()
            if key.startswith(SPLIT_FIELD):
                value_dict = {}
                for field in re.findall(r"\S+\(.+\)\s|\S+", value):
                    field = field.strip()
                    if field == "":
                        continue
                    if field.startswith("VideoQueue"):
                        k = "VideoQueue"
                        v = field[len("VideoQueue(") : -2]  # noqa: E203
                        fields = {}
                        v = v.split(" ")
                        for h in v:
                            f, vv = h.split("=")
                            fields[f] = vv
                        v = fields
                    else:
                        if "=" in field:
                            k, v = field.split("=", 1)
                        else:
                            k, v = field.split(":", 1)
                    value_dict[k] = v
                value = value_dict
            debug_info[key] = value
        res["mozRequestDebugInfo"] = debug_info
        return res

    def _dump_res(self, res):
        raw = json.dumps(res, indent=2, sort_keys=True)
        print(raw)
        if "upload_dir" in self.options:
            fn = "%s-videoPlaybackQuality.json" % self.video_id
            fn = os.path.join(self.options["upload_dir"], fn)
            # dumping on disk
            with open(fn, "w") as f:
                f.write(raw)

    def close(self):
        if self.started:
            self.marionette.delete_session()


@contextlib.contextmanager
def using_page(video_id, marionette, **options):
    page = YoutubePage(video_id, marionette, **options)
    try:
        yield page
    finally:
        page.close()
