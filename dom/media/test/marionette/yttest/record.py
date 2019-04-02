"""
MITM Script used to collect media files when a YT video is played.

This is a self-contained script that should not import anything else
except modules from the standard library and mitmproxy modules.
"""
import os


_HERE = os.path.dirname(__file__)
if "MOZPROXY_DIR" in os.environ:
    _DEFAULT_DATA_DIR = os.environ["MOZPROXY_DIR"]
else:
    _DEFAULT_DATA_DIR = os.path.join(_HERE, "..", "data")


def response(flow):
    print(flow.request.url)
    if "googlevideo.com/videoplayback" in flow.request.url:
        itag = flow.request.query["itag"]
        mime = flow.request.query["mime"].replace("/", "-")
        query_args = dict(flow.request.query)
        file_id = query_args["id"]
        file_range = query_args["range"]
        print("Writing %s:%s" % (file_id, file_range))
        # changing the host so the MITM recording file
        # does not rely on a specific YT server
        flow.request.host = "googlevideo.com"
        if len(flow.response.content) == 0:
            return
        path = "%s-%s-%s.%s" % (file_id, itag, file_range, mime)
        path = os.path.join(_DEFAULT_DATA_DIR, path)
        with open(path, "wb") as f:
            f.write(flow.response.content)
