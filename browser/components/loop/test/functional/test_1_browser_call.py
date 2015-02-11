from marionette_test import MarionetteTestCase
from by import By
import urlparse
from errors import NoSuchElementException, StaleElementException
# noinspection PyUnresolvedReferences
from wait import Wait

import os
import sys
sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

import pyperclip

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

    def wait_for_subelement_displayed(self, parent, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException])\
            .until(lambda m: parent.find_element(by, locator).is_displayed())
        return parent.find_element(by, locator)

    # XXX workaround for Marionette bug 1094246
    def wait_for_element_exists(self, by, locator, timeout=None):
        Wait(self.marionette, timeout,
             ignored_exceptions=[NoSuchElementException, StaleElementException]) \
            .until(lambda m: m.find_element(by, locator))
        return self.marionette.find_element(by, locator)

    def wait_for_element_enabled(self, element, timeout=10):
        Wait(self.marionette, timeout) \
            .until(lambda e: element.is_enabled(),
                   message="Timed out waiting for element to be enabled")

    def wait_for_element_attribute_to_be_false(self, element, attribute, timeout=10):
        Wait(self.marionette, timeout) \
            .until(lambda e: element.get_attribute(attribute) == "false",
                   message="Timeout out waiting for " + attribute + " to be false")

    def switch_to_panel(self):
        button = self.marionette.find_element(By.ID, "loop-button")

        # click the element
        button.click()

        # switch to the frame
        frame = self.marionette.find_element(By.ID, "loop-panel-iframe")
        self.marionette.switch_to_frame(frame)

    def switch_to_chatbox(self):
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()

        # XXX should be using wait_for_element_displayed, but need to wait
        # for Marionette bug 1094246 to be fixed.
        chatbox = self.wait_for_element_exists(By.TAG_NAME, 'chatbox')
        script = ("return document.getAnonymousElementByAttribute("
                  "arguments[0], 'class', 'chat-frame');")
        frame = self.marionette.execute_script(script, [chatbox])
        self.marionette.switch_to_frame(frame)

    def switch_to_standalone(self):
        self.marionette.set_context("content")

    def local_start_a_conversation(self):
        button = self.marionette.find_element(By.CSS_SELECTOR, ".rooms .btn-info")

        self.wait_for_element_enabled(button, 120)

        button.click()

    def local_check_room_self_video(self):
        self.switch_to_chatbox()

        # expect a video container on desktop side
        media_container = self.wait_for_element_displayed(By.CLASS_NAME, "media")
        self.assertEqual(media_container.tag_name, "div", "expect a video container")

    def local_get_and_verify_room_url(self):
        button = self.wait_for_element_displayed(By.CLASS_NAME, "btn-copy")

        button.click()

        # click the element
        room_url = pyperclip.paste()

        self.assertIn(urlparse.urlparse(room_url).scheme, ['http', 'https'],
                      "room URL returned by server " + room_url +
                      " has invalid scheme")
        return room_url

    def standalone_load_and_join_room(self, url):
        self.switch_to_standalone()
        self.marionette.navigate(url)

        # Join the room
        join_button = self.wait_for_element_displayed(By.CLASS_NAME,
                                                      "btn-join")
        join_button.click()

    # Assumes the standlone or the conversation window is selected first.
    def check_video(self, selector):
        video_wrapper = self.wait_for_element_displayed(By.CSS_SELECTOR,
                                                        selector, 20)
        video = self.wait_for_subelement_displayed(video_wrapper,
                                                   By.TAG_NAME, "video")

        self.wait_for_element_attribute_to_be_false(video, "paused")
        self.assertEqual(video.get_attribute("ended"), "false")

    def standalone_check_remote_video(self):
        self.switch_to_standalone()
        self.check_video(".remote .OT_subscriber .OT_widget-container")

    def local_check_remote_video(self):
        self.switch_to_chatbox()
        self.check_video(".remote .OT_subscriber .OT_widget-container")

    def local_enable_screenshare(self):
        self.switch_to_chatbox()
        button = self.marionette.find_element(By.CLASS_NAME, "btn-screen-share")

        button.click()

    def standalone_check_remote_screenshare(self):
        self.switch_to_standalone()
        self.check_video(".media .screen .OT_subscriber .OT_widget-container")

    def local_leave_room_and_verify_feedback(self):
        button = self.marionette.find_element(By.CLASS_NAME, "btn-hangup")

        button.click()

        # check that the feedback form is displayed
        feedback_form = self.wait_for_element_displayed(By.CLASS_NAME, "faces")
        self.assertEqual(feedback_form.tag_name, "div", "expect feedback form")

    def test_1_browser_call(self):
        self.switch_to_panel()

        self.local_start_a_conversation()

        # Check the self video in the conversation window
        self.local_check_room_self_video()

        room_url = self.local_get_and_verify_room_url()

        # load the link clicker interface into the current content browser
        self.standalone_load_and_join_room(room_url)

        # Check we get the video streams
        self.standalone_check_remote_video()
        self.local_check_remote_video()

        # XXX To enable this, we either need to navigate the permissions prompt
        # or have a route where we don't need the permissions prompt.
        # self.local_enable_screenshare()
        # self.standalone_check_remote_screenshare()

        # hangup the call
        self.local_leave_room_and_verify_feedback()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
