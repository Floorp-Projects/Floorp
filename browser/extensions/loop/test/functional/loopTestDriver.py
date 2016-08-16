from selenium import webdriver
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.common.by import By
from selenium.common.exceptions import NoSuchElementException, StaleElementReferenceException
import os

import re
import urlparse
import time
import pyperclip

from serversetup import ROOMS_WEB_APP_URL_BASE
from config import USE_LOCAL_STANDALONE, FIREFOX_PREFERENCES

DEFAULT_WAIT_TIMEOUT = 10


class LoopTestDriver():
    def setUp(self, extra_prefs={}):
        caps = DesiredCapabilities.FIREFOX

        # Tell the Python bindings to use Marionette.
        # This will not be necessary in the future,
        # when Selenium will auto-detect what remote end
        # it is talking to.
        caps["marionette"] = True

        # Path to Firefox DevEdition or Nightly.
        # Firefox 47 (stable) is currently not supported,
        # and may give you a suboptimal experience.
        #
        # On Mac OS you must point to the binary executable
        # inside the application package, such as
        # /Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin
        caps["binary"] = os.environ.get("FIREFOX_EXE")

        profile = webdriver.FirefoxProfile()

        # Although some of these preferences might require restart, we don't
        # use enforce_gecko_prefs (which would restart), as we need to restart
        # for the add-on installation anyway.
        for prefName, prefValue in FIREFOX_PREFERENCES.iteritems():
            profile.set_preference(prefName, prefValue)

        for prefName, prefValue in extra_prefs.iteritems():
            profile.set_preference(prefName, prefValue)

        profile.add_extension(os.environ.get("LOOP_XPI_FILE"))

        self.driver = webdriver.Firefox(capabilities=caps, firefox_profile=profile)

    # XXX This should be unnecessary once bug 1254132 is fixed.
    def set_context(self, context):
        self.context = context
        self.driver.set_context(context)

    def wait_for_element_displayed_by_class_name(self, locator, timeout=DEFAULT_WAIT_TIMEOUT):
        message = u"Couldn't find elem matching %s" % (locator)
        WebDriverWait(self.driver, timeout, poll_frequency=.25).until(
            lambda _: self.driver.find_element_by_class_name(locator), message=message)

        return self.driver.find_element_by_class_name(locator)

    def wait_for_element_displayed_by_css_selector(self, locator, timeout=DEFAULT_WAIT_TIMEOUT):
        message = u"Couldn't find elem matching %s" % (locator)
        WebDriverWait(self.driver, timeout, poll_frequency=.25).until(
            lambda _: self.driver.find_element_by_css_selector(locator), message=message)

        return self.driver.find_element_by_css_selector(locator)

    # XXX workaround for Marionette bug 1094246
    def wait_for_element_exists(self, by, locator, timeout=DEFAULT_WAIT_TIMEOUT):
        WebDriverWait(self.driver, timeout,
                      ignored_exceptions=[NoSuchElementException, StaleElementReferenceException]) \
            .until(lambda m: m.find_element(by, locator))
        return self.driver.find_element(by, locator)

    def wait_for_element_clickable(self, locator, timeout=DEFAULT_WAIT_TIMEOUT):
        WebDriverWait(self.driver, timeout, poll_frequency=.25) \
            .until(EC.element_to_be_clickable((By.CSS_SELECTOR, locator)),
                   message="Timed out waiting for element to be enabled")

    def wait_for_element_property_to_be_false(self, element, property, timeout=DEFAULT_WAIT_TIMEOUT):
        # import time
        # time.sleep(120)
        # XXX We have to switch between get_attribute and get_property here as the
        # content mode now required get_property for real properties of HTMLElements.
        # However, in some places (e.g. switch_to_chatbox), we're still operating in
        # a chrome mode. So we have to use get_attribute for these.
        # Bug 1277065 should fix this for marionette, alternately this should go
        # away when the e10s bug 1254132 is fixed.
        def check_property(m):
            if self.context == "content":
                return not element.get_property(property)
            print element.get_attribute(property)
            return element.get_attribute(property) == "false"

        WebDriverWait(self.driver, timeout, poll_frequency=.25) \
            .until(check_property,
                   message="Timeout out waiting for " + property + " to be false")

    def open_panel(self):
        self.set_context("chrome")
        self.driver.switch_to.default_content()
        button = self.driver.find_element_by_id("loop-button")

        # click the element
        button.click()

    def switch_to_panel(self):
        self.set_context("chrome")
        # switch to the frame
        frame = self.driver.find_element_by_id("loop-panel-iframe")
        self.driver.switch_to.frame(frame)

    def switch_to_chatbox(self):
        self.set_context("chrome")
        self.driver.switch_to.default_content()

        contentBox = "content"
        if self.e10s_enabled:
            contentBox = "remote-content"

        # Added time lapse to allow for DOM to catch up
        time.sleep(2)
        # XXX should be using wait_for_element_displayed, but need to wait
        # for Marionette bug 1094246 to be fixed.
        self.wait_for_element_exists(By.TAG_NAME, 'chatbox')

        script = ("var chatbox = document.getElementsByTagName('chatbox')[0];"
                  "return document.getAnonymousElementByAttribute("
                  "chatbox, 'anonid', '" + contentBox + "');")
        frame = self.driver.execute_script(script)
        self.driver.switch_to.frame(frame)

    def local_start_a_conversation(self):
        button = self.wait_for_element_displayed_by_css_selector(".new-room-view .btn-info")

        self.wait_for_element_clickable(".new-room-view .btn-info", 120)

        button.click()

    def local_close_share_panel(self):
        copyLink = self.wait_for_element_displayed_by_class_name("btn-copy")

        self.wait_for_element_clickable(".btn-copy", 120)

        copyLink.click()

    def switch_to_standalone(self):
        self.set_context("content")
        self.driver.switch_to.default_content()

    def load_homepage(self):
        self.switch_to_standalone()

        self.driver.get("about:home")

    def adjust_url(self, room_url):
        if USE_LOCAL_STANDALONE != "1":
            return room_url

        # If we're not using the local standalone, then we need to adjust the room
        # url that the server gives us to use the local standalone.
        return re.sub("https?://.*/", ROOMS_WEB_APP_URL_BASE + "/", room_url)

    def get_text_from_clipboard(self):
        return pyperclip.paste()

    def local_get_and_verify_room_url(self):
        self.switch_to_chatbox()
        button = self.wait_for_element_displayed_by_class_name("btn-copy")

        button.click()

        # click the element
        room_url = self.get_text_from_clipboard()

        room_url = self.adjust_url(room_url)

        self.assertIn(urlparse.urlparse(room_url).scheme, ['http', 'https'],
                      "room URL returned by server: '" + room_url +
                      "' has invalid scheme")
        return room_url

    # Assumes the standalone or the conversation window is selected first.
    def check_video(self, selector):
        # XXX This needs re-enabling once selenium is fixed
        # https://github.com/SeleniumHQ/selenium/issues/2301
        # video =
        self.wait_for_element_displayed_by_css_selector(selector, timeout=30)
        # self.wait_for_element_property_to_be_false(video, "paused")
        # self.wait_for_element_property_to_be_false(video, "ended")

    def local_check_room_self_video(self):
        self.switch_to_chatbox()

        # expect a video container on desktop side
        media_container = self.wait_for_element_displayed_by_class_name("media-layout")
        self.assertEqual(media_container.tag_name, "div", "expect a video container")

        self.check_video(".local-video")

    def standalone_load_and_join_room(self, url):
        self.switch_to_standalone()
        self.driver.get(url)

        # Join the room - the first time around, the tour will be displayed
        # so we look for its close button.
        join_button = self.wait_for_element_displayed_by_class_name("button-got-it")
        self.wait_for_element_clickable(".button-got-it", 120)
        join_button.click()
