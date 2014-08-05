from marionette_test import MarionetteTestCase
from marionette.errors import NoSuchElementException
from marionette.errors import StaleElementException
from by import By
# noinspection PyUnresolvedReferences
from marionette.wait import Wait
import os

# noinspection PyUnresolvedReferences
from mozprocess import processhandler
import urlparse

# XXX We want to convince ourselves that the race condition that importing
# hanging_threads.py works around, tracked by bug 1046873, is gone, and get
# rid of this inclusion entirely before we hook our functional tests up to
# Tbpl, so that we don't introduce an intermittent failure.
import sys
sys.path.append(os.path.dirname(__file__))
import hanging_threads

CONTENT_SERVER_PORT = 3001
LOOP_SERVER_PORT = 5001

CONTENT_SERVER_COMMAND = ["make", "runserver"]
CONTENT_SERVER_ENV = os.environ.copy()
# Set PORT so that it does not interfere with any other
# development server that might be running
CONTENT_SERVER_ENV.update({"PORT": str(CONTENT_SERVER_PORT),
                           "LOOP_SERVER_PORT": str(LOOP_SERVER_PORT)})

WEB_APP_URL = "http://localhost:" + str(CONTENT_SERVER_PORT) + \
              "/content/#call/{token}"

LOOP_SERVER_COMMAND = ["make", "runserver"]
LOOP_SERVER_ENV = os.environ.copy()
# Set PORT so that it does not interfere with any other
# development server that might be running
LOOP_SERVER_ENV.update({"NODE_ENV": "dev", "PORT": str(LOOP_SERVER_PORT),
                        "WEB_APP_URL": WEB_APP_URL})


class LoopTestServers:
    def __init__(self):
        self.loop_server = self.start_loop_server()
        self.content_server = self.start_content_server()

    @staticmethod
    def start_loop_server():
        loop_server_location = os.environ.get('LOOP_SERVER')
        if loop_server_location is None:
            raise Exception('LOOP_SERVER variable not set')

        os.chdir(loop_server_location)

        p = processhandler.ProcessHandler(LOOP_SERVER_COMMAND,
                                          env=LOOP_SERVER_ENV)
        p.run()
        return p

    @staticmethod
    def start_content_server():
        content_server_location = os.path.join(os.path.dirname(__file__),
                                               "../../standalone")
        os.chdir(content_server_location)

        p = processhandler.ProcessHandler(CONTENT_SERVER_COMMAND,
                                          env=CONTENT_SERVER_ENV)
        p.run()
        return p

    def shutdown(self):
        self.content_server.kill()
        self.loop_server.kill()


class TestGetUrl(MarionetteTestCase):
    # XXX Move this to setup class so it doesn't restart the server
    # after every test.  This can happen when we write our second test,
    # expected to be in bug 976116.
    def setUp(self):
        # start server
        self.loop_test_servers = LoopTestServers()

        MarionetteTestCase.setUp(self)

        # Unfortunately, enforcing preferences currently comes with the side
        # effect of launching and restarting the browser before running the
        # real functional tests.  Bug 1048554 has been filed to track this.
        preferences = {"loop.server": "http://localhost:" + str(LOOP_SERVER_PORT)}
        self.marionette.enforce_gecko_prefs(preferences)

        # this is browser chrome, kids, not the content window just yet
        self.marionette.set_context("chrome")

    def switch_to_panel(self):
        button = self.marionette.find_element(By.ID, "loop-call-button")

        # click the element
        button.click()

        # switch to the frame
        frame = self.marionette.find_element(By.ID, "loop")
        self.marionette.switch_to_frame(frame)

    # taken from https://github.com/mozilla-b2g/gaia/blob/master/tests/python/gaia-ui-tests/gaiatest/gaia_test.py#L858
    # XXX factor out into utility object for use by other tests, this can
    # when we write our second test, in bug 976116
    def wait_for_element_displayed(self, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException])\
            .until(lambda m: m.find_element(by, locator).is_displayed())
        return self.marionette.find_element(by, locator)

    def test_get_url(self):
        self.switch_to_panel()

        # get and check for a call url
        url_input_element = self.wait_for_element_displayed(By.TAG_NAME,
                                                            "input")

        # wait for pending state to finish
        self.assertEqual(url_input_element.get_attribute("class"), "pending",
                         "expect the input to be pending")

        # get and check the input (the "callUrl" class is only added after
        # the pending class is removed and the URL has arrived).
        #
        # XXX should investigate getting rid of the fragile and otherwise
        # unnecessary callUrl class and replacing this with a By.CSS_SELECTOR
        # and some possible combination of :not and/or an attribute selector
        # once bug 1048551 is fixed.
        url_input_element = self.wait_for_element_displayed(By.CLASS_NAME,
                                                            "callUrl")
        call_url = url_input_element.get_attribute("value")
        self.assertNotEqual(call_url, u'',
                            "input is populated with call URL after pending"
                            " is finished")

        self.assertIn(urlparse.urlparse(call_url).scheme, ['http', 'https'],
                      "call URL returned by server " + call_url +
                      " has invalid scheme")

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
