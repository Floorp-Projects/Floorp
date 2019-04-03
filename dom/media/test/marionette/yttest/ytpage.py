# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Drives the browser during the playback test.
"""
import contextlib
import os


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
        options = dict(JS_MACROS)
        options.update(self.options)
        if "duration" in options:
            script = DURATION_TEST % options
        else:
            script = UNTIL_END_TEST % options
        self.marionette.set_pref("media.autoplay.default", 0)
        return self.execute_async_script(script)

    def execute_async_script(self, script, context=None):
        if context is None:
            context = self.marionette.CONTEXT_CONTENT
        with self.marionette.using_context(context):
            return self.marionette.execute_async_script(script, sandbox="system")

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
