import unittest

import os
import sys
sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

from serversetup import LoopTestServers
from loopTestDriver import LoopTestDriver


class Test1BrowserCall(LoopTestDriver, unittest.TestCase):
    # XXX Move this to setup class so it doesn't restart the server
    # after every test.
    def setUp(self):
        # start server
        self.loop_test_servers = LoopTestServers()

        LoopTestDriver.setUp(self)

        self.e10s_enabled = os.environ.get("TEST_E10S") == "1"

        # this is browser chrome, kids, not the content window just yet
        self.driver.set_context("chrome")

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
        chatbox = self.wait_for_element_displayed_by_css_selector(".text-chat-box > form > input")

        chatbox.send_keys(text + "\n")

    def check_received_message(self, expectedText):
        """
        Checks a chat message has been received in the current context. The
        test assumes only one chat message will be received during the tests.

        :param expectedText: The expected text of the chat message.
        """
        text_entry = self.wait_for_element_displayed_by_css_selector(".text-chat-entry.received > p")

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
        button = self.driver.find_element_by_class_name("btn-hangup")

        button.click()

        self.switch_to_chatbox()
        # check that the local view reverts to the preview mode
        self.wait_for_element_displayed_by_class_name("room-invitation-content")

    def local_leave_room(self):
        button = self.marionette.find_element_by_class_name("stop-sharing-button")

        button.click()

    def local_close_conversation(self):
        self.set_context("chrome")
        self.driver.switch_to.default_content()

        script = ("var chatbox = document.getElementsByTagName('chatbox')[0];"
                  "return document.getAnonymousElementByAttribute("
                  "chatbox, 'class', 'chat-loop-hangup chat-toolbarbutton');")
        close_button = self.driver.execute_script(script)

        close_button.click()

    def check_feedback_form(self):
        self.switch_to_chatbox()

        feedbackPanel = self.wait_for_element_displayed_by_css_selector(".feedback-view-container")
        self.assertNotEqual(feedbackPanel, "")

    def check_rename_layout(self):
        self.switch_to_panel()

        renameInput = self.wait_for_element_displayed_by_css_selector(".rename-input")
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

        room_url = self.local_get_and_verify_room_url()

        # load the link clicker interface into the current content browser
        self.standalone_load_and_join_room(room_url)

        # Check we get the video streams
        self.standalone_check_remote_video()
        self.local_check_remote_video()

        # Check text messaging
        self.check_text_messaging()

        # Check that screenshare was automatically started
        self.standalone_check_remote_screenshare()

        # We hangup on the remote (standalone) side, because this also leaves
        # the local chatbox with the local publishing media still connected,
        # which means that the local_check_connection_length below
        # verifies that the connection is noted at the time the remote media
        # drops, rather than waiting until the window closes.
        self.remote_leave_room()

        # Hangup on local will open feedback window first
        self.local_close_conversation()
        self.check_feedback_form()
        # Close the window once again to see the rename layout
        self.local_close_conversation()
        self.check_rename_layout()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        self.driver.quit()

if __name__ == "__main__":
    unittest.main()
