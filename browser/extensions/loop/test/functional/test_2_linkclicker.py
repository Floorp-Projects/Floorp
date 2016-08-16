import unittest

import os
import sys
sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))

from serversetup import LoopTestServers
from loopTestDriver import LoopTestDriver
from config import CONTENT_SERVER_URL


class Test2Linkclicker(LoopTestDriver, unittest.TestCase):
    # XXX Move this to setup class so it doesn't restart the server
    # after every test.
    def setUp(self):
        # start server
        self.loop_test_servers = LoopTestServers()

        # Set the standalone url for these tests.
        standalone_url = CONTENT_SERVER_URL + "/"
        LoopTestDriver.setUp(self, extra_prefs={
            "loop.linkClicker.url": standalone_url,
            "webchannel.allowObject.urlWhitelist": CONTENT_SERVER_URL
        })

        self.e10s_enabled = os.environ.get("TEST_E10S") == "1"

        # this is browser chrome, kids, not the content window just yet
        self.driver.set_context("chrome")

    def navigate_to_standalone(self, url):
        self.switch_to_standalone()
        self.driver.get(url)

    def standalone_check_own_link_view(self):
        view_container = self.wait_for_element_displayed_by_css_selector(
            ".handle-user-agent-view-scroller", 30)
        self.assertEqual(view_container.tag_name, "div", "expect a error container")

    def local_leave_room(self):
        self.open_panel()
        self.switch_to_panel()
        button = self.driver.find_element_by_class_name("stop-sharing-button")

        button.click()

    def standalone_join_own_room(self):
        button = self.wait_for_element_displayed_by_class_name("btn-info", 30)

        button.click()

    def standalone_check_error_text(self):
        error_container = self.wait_for_element_displayed_by_class_name("failure", 30)
        self.assertEqual(error_container.tag_name, "p", "expect a error container")

    def test_2_own_room_test(self):
        # Marionette doesn't make it easy to set up a page to load automatically
        # on start. So lets load about:home. We need this for now, so that the
        # various browser checks believe we've enabled e10s.
        self.load_homepage()

        self.open_panel()
        self.switch_to_panel()

        self.local_start_a_conversation()

        # Force to close the share panel
        self.local_close_share_panel()

        room_url = self.local_get_and_verify_room_url()

        self.local_leave_room()

        # load the link clicker interface into the current content browser
        self.navigate_to_standalone(room_url)

        self.standalone_check_own_link_view()

        self.standalone_join_own_room()

        # Ensure we're on a different page so that we can navigate back to the room url later.
        self.load_homepage()

        self.local_check_room_self_video()

        # reload the link clicker interface into the current content browser
        self.navigate_to_standalone(room_url)

        self.standalone_join_own_room()

        self.standalone_check_error_text()

    def tearDown(self):
        self.loop_test_servers.shutdown()
        self.driver.quit()

if __name__ == "__main__":
    unittest.main()
