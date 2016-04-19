# taken from https://github.com/mozilla-b2g/gaia/blob/master/tests/python/gaia-ui-tests/gaiatest/gaia_test.py#L858
# XXX factor out into utility object for use by other tests
from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, StaleElementException
# noinspection PyUnresolvedReferences
from marionette_driver import Wait

import pyperclip
import re
import urlparse
import time

from serversetup import ROOMS_WEB_APP_URL_BASE
from config import USE_LOCAL_STANDALONE


class LoopTestDriver():
    def setUp(self, marionette):
        self.marionette = marionette

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

    def open_panel(self):
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()
        button = self.marionette.find_element(By.ID, "loop-button")

        # click the element
        button.click()

    def switch_to_panel(self):
        self.marionette.set_context("chrome")
        # switch to the frame
        frame = self.marionette.find_element(By.ID, "loop-panel-iframe")
        self.marionette.switch_to_frame(frame)

    def switch_to_chatbox(self):
        self.marionette.set_context("chrome")
        self.marionette.switch_to_frame()

        contentBox = "content"
        if self.e10s_enabled:
            contentBox = "remote-content"

        # Added time lapse to allow for DOM to catch up
        time.sleep(2)
        # XXX should be using wait_for_element_displayed, but need to wait
        # for Marionette bug 1094246 to be fixed.
        chatbox = self.wait_for_element_exists(By.TAG_NAME, 'chatbox')
        script = ("return document.getAnonymousElementByAttribute("
                  "arguments[0], 'anonid', '" + contentBox + "');")
        frame = self.marionette.execute_script(script, [chatbox])
        self.marionette.switch_to_frame(frame)

    def local_start_a_conversation(self):
        button = self.wait_for_element_displayed(By.CSS_SELECTOR, ".new-room-view .btn-info")

        self.wait_for_element_enabled(button, 120)

        button.click()

    def local_close_share_panel(self):
        copyLink = self.wait_for_element_displayed(By.CLASS_NAME, "btn-copy")

        self.wait_for_element_enabled(copyLink, 120)

        copyLink.click()

    def switch_to_standalone(self):
        self.marionette.set_context("content")

    def load_homepage(self):
        self.switch_to_standalone()

        self.marionette.navigate("about:home")

    def adjust_url(self, room_url):
        if USE_LOCAL_STANDALONE != "1":
            return room_url

        # If we're not using the local standalone, then we need to adjust the room
        # url that the server gives us to use the local standalone.
        return re.sub("https?://.*/", ROOMS_WEB_APP_URL_BASE + "/", room_url)

    def local_get_and_verify_room_url(self):
        self.switch_to_chatbox()
        button = self.wait_for_element_displayed(By.CLASS_NAME, "btn-copy")

        button.click()

        # click the element
        room_url = pyperclip.paste()

        room_url = self.adjust_url(room_url)

        self.assertIn(urlparse.urlparse(room_url).scheme, ['http', 'https'],
                      "room URL returned by server: '" + room_url +
                      "' has invalid scheme")
        return room_url

    # Assumes the standalone or the conversation window is selected first.
    def check_video(self, selector):
        video = self.wait_for_element_displayed(By.CSS_SELECTOR, selector, 30)
        self.wait_for_element_attribute_to_be_false(video, "paused")
        self.assertEqual(video.get_attribute("ended"), "false")

    def local_check_room_self_video(self):
        self.switch_to_chatbox()

        # expect a video container on desktop side
        media_container = self.wait_for_element_displayed(By.CLASS_NAME, "media-layout")
        self.assertEqual(media_container.tag_name, "div", "expect a video container")

        self.check_video(".local-video")

    def standalone_load_and_join_room(self, url):
        self.switch_to_standalone()
        self.marionette.navigate(url)

        # Join the room - the first time around, the tour will be displayed
        # so we look for its close button.
        join_button = self.wait_for_element_displayed(By.CLASS_NAME, "button-got-it")
        self.wait_for_element_enabled(join_button, 120)
        join_button.click()
