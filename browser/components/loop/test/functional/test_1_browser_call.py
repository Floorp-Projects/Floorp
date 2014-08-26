from marionette_test import MarionetteTestCase
from by import By
import urlparse
from marionette.errors import NoSuchElementException, StaleElementException
# noinspection PyUnresolvedReferences
from marionette.wait import Wait
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

    # XXX workaround for Marionette bug YYY
    def wait_for_element_exists(self, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException]) \
            .until(lambda m: m.find_element(by, locator))
        return self.marionette.find_element(by, locator)

    def switch_to_panel(self):
        button = self.marionette.find_element(By.ID, "loop-call-button")

        # click the element
        button.click()

        # switch to the frame
        frame = self.marionette.find_element(By.ID, "loop")
        self.marionette.switch_to_frame(frame)

    def load_and_verify_standalone_ui(self, url):
        self.marionette.set_context("content")
        self.marionette.navigate(url)

        call_url_link = self.marionette.find_element(By.CLASS_NAME, "call-url") \
            .text
        self.assertEqual(url, call_url_link,
                         "should be on the correct page")

    def get_and_verify_call_url(self):
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
        return call_url

    def start_and_verify_outgoing_call(self):
        # make the call!
        call_button = self.marionette.find_element(By.CLASS_NAME, "btn-success")
        call_button.click()

        # expect a video container on standalone side
        video = self.wait_for_element_displayed(By.CLASS_NAME, "media")
        self.assertEqual(video.tag_name, "div", "expect a video container")

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

        # Accept the incoming call
        call_button = self.marionette.find_element(By.CLASS_NAME,
                                                   "btn-success")
        # accept call from the desktop side
        call_button.click()

        # expect a video container on desktop side
        video = self.wait_for_element_displayed(By.CLASS_NAME, "media")
        self.assertEqual(video.tag_name, "div", "expect a video container")

    def hangup_call_and_verify_feedback(self):
        self.marionette.set_context("chrome")
        button = self.marionette.find_element(By.CLASS_NAME, "btn-hangup")

        # XXX For whatever reason, the click doesn't take effect unless we
        # wait for a bit (even if we wait for the element to actually be
        # displayed first, which we're not currently bothering with).  It's
        # not entirely clear whether the click is being delivered in this case,
        # or whether there's a Marionette bug here.
        sleep(2)
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

        # hangup the call
        self.hangup_call_and_verify_feedback()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
