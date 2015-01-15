from marionette_test import MarionetteTestCase
from by import By
import urlparse
from errors import NoSuchElementException, StaleElementException
# noinspection PyUnresolvedReferences
from wait import Wait
from time import sleep

import os
import sys
sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

from serversetup import LoopTestServers
from config import *


class Test1BrowserCall(MarionetteTestCase):
    # XXX Move this to setup class so it doesn't restart the server
    # after every test.
    def setUp(self):
        # start server
        self.loop_test_servers = LoopTestServers()

        MarionetteTestCase.setUp(self)

        # Unfortunately, enforcing preferences currently comes with the side
        # effect of launching and restarting the browser before running the
        # real functional tests.  Bug 1048554 has been filed to track this.
        self.marionette.enforce_gecko_prefs(FIREFOX_PREFERENCES)

        # this is browser chrome, kids, not the content window just yet
        self.marionette.set_context("chrome")

    # taken from https://github.com/mozilla-b2g/gaia/blob/master/tests/python/gaia-ui-tests/gaiatest/gaia_test.py#L858
    # XXX factor out into utility object for use by other tests
    def wait_for_element_displayed(self, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException])\
            .until(lambda m: m.find_element(by, locator).is_displayed())
        return self.marionette.find_element(by, locator)

    # XXX workaround for Marionette bug 1055309
    def wait_for_element_exists(self, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException]) \
            .until(lambda m: m.find_element(by, locator))
        return self.marionette.find_element(by, locator)

    def switch_to_panel(self):
        button = self.marionette.find_element(By.ID, "loop-button")

        # click the element
        button.click()

        # switch to the frame
        frame = self.marionette.find_element(By.ID, "loop-panel-iframe")
        self.marionette.switch_to_frame(frame)

    def load_and_verify_standalone_ui(self, url):
        self.marionette.set_context("content")
        self.marionette.navigate(url)

    def start_a_conversation(self):
        # TODO: wait for react elements
        sleep(2)
        button = self.marionette.find_element(By.CSS_SELECTOR, ".rooms .btn-info")

        # click the element
        button.click()

    def get_and_verify_call_url(self):
        # in the new room model we have to first start a conversation
        self.start_a_conversation()

        # TODO: wait for react elements
        sleep(2)
        call_url = self.marionette.find_element(By.CLASS_NAME, \
                                                "room-url-link").text

        self.assertIn(urlparse.urlparse(call_url).scheme, ['http', 'https'],
                      "call URL returned by server " + call_url +
                      " has invalid scheme")
        return call_url

    def start_and_verify_outgoing_call(self):
        # TODO: wait for react elements
        sleep(2)
        # make the call!
        call_button = self.marionette.find_element(By.CLASS_NAME,
                                                   "btn-join")
        call_button.click()

    def accept_and_verify_incoming_call(self):
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()

        # XXX should be using wait_for_element_displayed, but need to wait
        # for Marionette bug 1055309 to be fixed.
        chatbox = self.wait_for_element_exists(By.TAG_NAME, 'chatbox')
        script = ("return document.getAnonymousElementByAttribute("
                  "arguments[0], 'class', 'chat-frame');")
        frame = self.marionette.execute_script(script, [chatbox])
        self.marionette.switch_to_frame(frame)

        # expect a video container on desktop side
        video = self.wait_for_element_displayed(By.CLASS_NAME, "media")
        self.assertEqual(video.tag_name, "div", "expect a video container")

    def hangup_call_and_verify_feedback(self):
        self.marionette.set_context("chrome")
        button = self.marionette.find_element(By.CLASS_NAME, "btn-hangup")

        # XXX bug 1080095 For whatever reason, the click doesn't take effect
        # unless we wait for a bit (even if we wait for the element to
        # actually be displayed first, which we're not currently bothering
        # with).  It's not entirely clear whether the click is being
        # delivered in this case, or whether there's a Marionette bug here.
        sleep(5)
        button.click()

        # check that the feedback form is displayed
        feedback_form = self.wait_for_element_displayed(By.CLASS_NAME, "faces")
        self.assertEqual(feedback_form.tag_name, "div", "expect feedback form")

    def test_1_browser_call(self):
        self.switch_to_panel()

        call_url = self.get_and_verify_call_url()

        # load the link clicker interface into the current content browser
        self.load_and_verify_standalone_ui(call_url)

        self.start_and_verify_outgoing_call()

        # Switch to the conversation window and answer
        self.accept_and_verify_incoming_call()

        # Let's wait for the call/media to get established.
        # TODO: replace this with some media detection
        sleep(5)

        # hangup the call
        self.hangup_call_and_verify_feedback()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
