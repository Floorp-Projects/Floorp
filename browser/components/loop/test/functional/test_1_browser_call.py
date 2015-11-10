from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, StaleElementException
# noinspection PyUnresolvedReferences
from marionette_driver import Wait
from marionette import MarionetteTestCase

import os
import sys
import urlparse
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
        button = self.wait_for_element_displayed(By.CSS_SELECTOR, ".new-room-view .btn-info")

        self.wait_for_element_enabled(button, 120)

        button.click()

    def local_check_room_self_video(self):
        self.switch_to_chatbox()

        # expect a video container on desktop side
        media_container = self.wait_for_element_displayed(By.CLASS_NAME, "media-layout")
        self.assertEqual(media_container.tag_name, "div", "expect a video container")

        self.check_video(".local-video")

    def local_get_and_verify_room_url(self):
        self.switch_to_chatbox()
        button = self.wait_for_element_displayed(By.CLASS_NAME, "btn-copy")

        button.click()

        # click the element
        room_url = pyperclip.paste()

        self.assertIn(urlparse.urlparse(room_url).scheme, ['http', 'https'],
                      "room URL returned by server: '" + room_url +
                      "' has invalid scheme")
        return room_url

    def standalone_load_and_join_room(self, url):
        self.switch_to_standalone()
        self.marionette.navigate(url)

        # Join the room
        join_button = self.wait_for_element_displayed(By.CLASS_NAME,
                                                      "btn-join")
        join_button.click()

    # Assumes the standalone or the conversation window is selected first.
    def check_video(self, selector):
        video = self.wait_for_element_displayed(By.CSS_SELECTOR,
                                                        selector, 20)
        self.wait_for_element_attribute_to_be_false(video, "paused")
        self.assertEqual(video.get_attribute("ended"), "false")

    def standalone_check_remote_video(self):
        self.switch_to_standalone()
        self.check_video(".remote-video")

    def local_check_remote_video(self):
        self.switch_to_chatbox()
        self.check_video(".remote-video")

    def send_chat_message(self, text):
        """
        Sends a chat message using the current context.

        :param text: The text to send.
        """
        chatbox = self.wait_for_element_displayed(By.CSS_SELECTOR,
                                                  ".text-chat-box > form > input")

        chatbox.send_keys(text + "\n")

    def check_received_message(self, expectedText):
        """
        Checks a chat message has been received in the current context. The
        test assumes only one chat message will be received during the tests.

        :param expectedText: The expected text of the chat message.
        """
        text_entry = self.wait_for_element_displayed(By.CSS_SELECTOR,
                                                     ".text-chat-entry.received > p > span")

        self.assertEqual(text_entry.text, expectedText,
                         "should have received the correct message")

    def check_text_messaging(self):
        """
        Checks text messaging between the generator and clicker in a bi-directional
        fashion.
        """
        # Send a message using the link generator.
        self.switch_to_chatbox()
        self.send_chat_message("test1")

        # Now check the result on the link clicker.
        self.switch_to_standalone()
        self.check_received_message("test1")

        # Then send a message using the standalone.
        self.send_chat_message("test2")

        # Finally check the link generator got it.
        self.switch_to_chatbox()
        self.check_received_message("test2")

    def local_enable_screenshare(self):
        self.switch_to_chatbox()
        button = self.marionette.find_element(By.CLASS_NAME, "btn-screen-share")

        button.click()

    def standalone_check_remote_screenshare(self):
        self.switch_to_standalone()
        self.check_video(".screen-share-video")

    def remote_leave_room(self):
        self.switch_to_standalone()
        button = self.marionette.find_element(By.CLASS_NAME, "btn-hangup")

        button.click()

        self.switch_to_chatbox()
        # check that the local view reverts to the preview mode
        self.wait_for_element_displayed(By.CLASS_NAME, "room-invitation-content")

    def local_get_chatbox_window_expr(self, expr):
        """
        :expr: a sub-expression which must begin with a property of the
        global content window (e.g. "location.path")

        :return: the value of the given sub-expression as evaluated in the
        chatbox content window
        """
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()

        # XXX should be using wait_for_element_displayed, but need to wait
        # for Marionette bug 1094246 to be fixed.
        chatbox = self.wait_for_element_exists(By.TAG_NAME, 'chatbox')
        script = '''
            let chatBrowser = document.getAnonymousElementByAttribute(
              arguments[0], 'class',
              'chat-frame')

            // note that using wrappedJSObject waives X-ray vision, which
            // has security implications, but because we trust the code
            // running in the chatbox, it should be reasonably safe
            let chatGlobal = chatBrowser.contentWindow.wrappedJSObject;

            return chatGlobal.''' + expr

        return self.marionette.execute_script(script, [chatbox])

    def local_get_media_start_time(self):
        return self.local_get_chatbox_window_expr(
            "loop.conversation._sdkDriver._getTwoWayMediaStartTime()")

    # XXX could be memoized
    def local_get_media_start_time_uninitialized(self):
        return self.local_get_chatbox_window_expr(
            "loop.conversation._sdkDriver.CONNECTION_START_TIME_UNINITIALIZED"
        )

    def local_check_media_start_time_uninitialized(self):
        self.assertEqual(
            self.local_get_media_start_time(),
            self.local_get_media_start_time_uninitialized(),
            "media start time should be uninitialized before "
            "link clicker enters room")

    def local_check_media_start_time_initialized(self):
        self.assertNotEqual(
            self.local_get_media_start_time(),
            self.local_get_media_start_time_uninitialized(),
            "media start time should be initialized after "
            "media is bidirectionally connected")

    def local_check_connection_length_noted(self):
        noted_calls = self.local_get_chatbox_window_expr(
            "loop.conversation._sdkDriver._connectionLengthNotedCalls")

        self.assertGreater(noted_calls, 0,
                           "OTSdkDriver._connectionLengthNotedCalls should be "
                           "> 0, noted_calls = " + str(noted_calls))

    def test_1_browser_call(self):
        self.switch_to_panel()

        self.local_start_a_conversation()

        # Check the self video in the conversation window
        self.local_check_room_self_video()

        # make sure that the media start time is not initialized
        self.local_check_media_start_time_uninitialized()

        room_url = self.local_get_and_verify_room_url()

        # load the link clicker interface into the current content browser
        self.standalone_load_and_join_room(room_url)

        # Check we get the video streams
        self.standalone_check_remote_video()
        self.local_check_remote_video()

        # Check text messaging
        self.check_text_messaging()

        # since bi-directional media is connected, make sure we've set
        # the start time
        self.local_check_media_start_time_initialized()

        # XXX To enable this, we either need to navigate the permissions prompt
        # or have a route where we don't need the permissions prompt.
        # self.local_enable_screenshare()
        # self.standalone_check_remote_screenshare()

        # We hangup on the remote (standalone) side, because this also leaves
        # the local chatbox with the local publishing media still connected,
        # which means that the local_check_connection_length below
        # verifies that the connection is noted at the time the remote media
        # drops, rather than waiting until the window closes.
        self.remote_leave_room()

        self.local_check_connection_length_noted()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
