# Pull down and configure the loop server
# Start the Loop server
# Start the standalone server
# Other util functions

# noinspection PyUnresolvedReferences
from mozprocess import processhandler

# XXX We want to convince ourselves that the race condition that importing
# hanging_threads.py works around, tracked by bug 1046873, is gone, and get
# rid of this inclusion entirely before we hook our functional tests up to
# Tbpl, so that we don't introduce an intermittent failure.
import sys
import os
sys.path.append(os.path.dirname(__file__))
import hanging_threads
from config import CONTENT_SERVER_PORT, CONTENT_SERVER_URL, LOOP_SERVER_PORT, LOOP_SERVER_URL, \
    FIREFOX_PREFERENCES, TEST_SERVER, USE_LOCAL_STANDALONE

hanging_threads.start_monitoring()

WORKING_DIR = os.getcwd()

CONTENT_SERVER_COMMAND = ["make", "runserver_nowatch"]
CONTENT_SERVER_ENV = os.environ.copy()
# Set PORT so that it does not interfere with any other
# development server that might be running
CONTENT_SERVER_ENV.update({"PORT": str(CONTENT_SERVER_PORT),
                           "LOOP_SERVER_URL": LOOP_SERVER_URL})

ROOMS_WEB_APP_URL_BASE = CONTENT_SERVER_URL

ROOMS_WEB_APP_URL = ROOMS_WEB_APP_URL_BASE + \
    "/{token}"

LOOP_SERVER_COMMAND = ["make", "runserver"]
LOOP_SERVER_ENV = os.environ.copy()
# Set PORT so that it does not interfere with any other
# development server that might be running
LOOP_SERVER_ENV.update({"NODE_ENV": "dev",
                        "PORT": str(LOOP_SERVER_PORT),
                        "SERVER_ADDRESS": LOOP_SERVER_URL,
                        "ROOMS_WEB_APP_URL": ROOMS_WEB_APP_URL})


class LoopTestServers:
    def __init__(self):
        if TEST_SERVER == "local":
            loop_server_location = os.environ.get('LOOP_SERVER')
            if not loop_server_location:
                raise Exception('LOOP_SERVER variable not set')

            if loop_server_location.startswith("http"):
                FIREFOX_PREFERENCES["loop.server"] = loop_server_location
                return

            self.loop_server = self.start_loop_server(loop_server_location)

        if USE_LOCAL_STANDALONE == "1":
            self.content_server = self.start_content_server()

    @staticmethod
    def start_loop_server(loop_server_location):
        if loop_server_location is None:
            raise Exception('LOOP_SERVER variable not set')

        os.chdir(loop_server_location)

        p = processhandler.ProcessHandler(LOOP_SERVER_COMMAND,
                                          env=LOOP_SERVER_ENV)
        p.run()
        return p

    @staticmethod
    def start_content_server():
        content_server_location = os.environ.get('STANDALONE_SERVER')
        if content_server_location is None:
            content_server_location = WORKING_DIR

        os.chdir(content_server_location)

        p = processhandler.ProcessHandler(CONTENT_SERVER_COMMAND,
                                          env=CONTENT_SERVER_ENV)
        p.run()

        # Give the content server time to start.
        import time
        output = p.output
        while not output:
            time.sleep(1)
            output = p.output

        return p

    def shutdown(self):
        if hasattr(self, "content_server"):
            self.content_server.kill()
        if hasattr(self, "loop_server"):
            self.loop_server.kill()
