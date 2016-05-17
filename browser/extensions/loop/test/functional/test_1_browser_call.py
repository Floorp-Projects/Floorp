from marionette_driver.by import By
# noinspection PyUnresolvedReferences
from marionette_driver.addons import Addons
from marionette import MarionetteTestCase

import os
import sys
sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

from serversetup import LoopTestServers
from config import FIREFOX_PREFERENCES

from loopTestDriver import LoopTestDriver


class Test1BrowserCall(LoopTestDriver, MarionetteTestCase):
    # XXX Move this to setup class so it doesn't restart the server
    # after every test.
    def setUp(self):
        # start server
        self.loop_test_servers = LoopTestServers()

        MarionetteTestCase.setUp(self)
        LoopTestDriver.setUp(self, self.marionette)

        # Although some of these preferences might require restart, we don't
        # use enforce_gecko_prefs (which would restart), as we need to restart
        # for the add-on installation anyway.
        self.marionette.set_prefs(FIREFOX_PREFERENCES)

        xpi_file = os.environ.get("LOOP_XPI_FILE")

        if xpi_file:
            addons = Addons(self.marionette)
            # XXX We should really use temp=True here, but due to the later
            # restart to ensure the add-on is installed correctly, this won't work
            # at the moment. What we need is a fully restartless add-on - bug 1229352
            # at which point we should be able to make this install temporarily
            # and after the restart.
            addons.install(os.path.abspath(xpi_file))

        self.e10s_enabled = os.environ.get("TEST_E10S") == "1"

        # Restart the browser nicely, so the preferences and add-on installation
        # take full effect.
        self.marionette.restart(in_app=True)

        # this is browser chrome, kids, not the content window just yet
        self.marionette.set_context("chrome")

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
                                                     ".text-chat-entry.received > p")

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

    def local_leave_room(self):
        button = self.marionette.find_element(By.CLASS_NAME, "stop-sharing-button")

        button.click()

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
              arguments[0], 'anonid',
              'content')

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

    def local_close_conversation(self):
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()

        chatbox = self.wait_for_element_exists(By.TAG_NAME, 'chatbox')
        close_button = chatbox.find_element(By.ANON_ATTRIBUTE, {"class": "chat-loop-hangup chat-toolbarbutton"})

        close_button.click()

    def check_feedback_form(self):
        self.switch_to_chatbox()

        feedbackPanel = self.wait_for_element_displayed(By.CSS_SELECTOR, ".feedback-view-container")
        self.assertNotEqual(feedbackPanel, "")

    def check_rename_layout(self):
        self.switch_to_panel()

        renameInput = self.wait_for_element_displayed(By.CSS_SELECTOR, ".rename-input")
        self.assertNotEqual(renameInput, "")

    def test_1_browser_call(self):
        # Marionette doesn't make it easy to set up a page to load automatically
        # on start. So lets load about:home. We need this for now, so that the
        # various browser checks believe we've enabled e10s.
        self.load_homepage()
        self.open_panel()
        self.switch_to_panel()

        self.local_start_a_conversation()

        # Force to close the share panel
        self.local_close_share_panel()

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

        # Check that screenshare was automatically started
        self.standalone_check_remote_screenshare()

        # We hangup on the remote (standalone) side, because this also leaves
        # the local chatbox with the local publishing media still connected,
        # which means that the local_check_connection_length below
        # verifies that the connection is noted at the time the remote media
        # drops, rather than waiting until the window closes.
        self.remote_leave_room()

        self.local_check_connection_length_noted()

        # Hangup on local will open feedback window first
        self.local_close_conversation()
        self.check_feedback_form()
        # Close the window once again to see the rename layout
        self.local_close_conversation()
        self.check_rename_layout()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        MarionetteTestCase.tearDown(self)
