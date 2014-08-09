# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from by import By
from marionette import Actions
from marionette_test import MarionetteTestCase
from selection import SelectionManager


class TouchCaretTest(MarionetteTestCase):
    _input_selector = (By.ID, 'input')
    _textarea_selector = (By.ID, 'textarea')
    _contenteditable_selector = (By.ID, 'contenteditable')
    _large_expiration_time = 3000 * 20  # 60 seconds

    def setUp(self):
        # Code to execute before a test is being run.
        MarionetteTestCase.setUp(self)
        self.actions = Actions(self.marionette)
        self.original_expiration_time = self.expiration_time

    def tearDown(self):
        # Code to execute after a test is being run.
        self.expiration_time = self.original_expiration_time
        MarionetteTestCase.tearDown(self)

    @property
    def expiration_time(self):
        'Return touch caret expiration time in milliseconds.'
        return self.marionette.execute_script(
            'return SpecialPowers.getIntPref("touchcaret.expiration.time");')

    @expiration_time.setter
    def expiration_time(self, expiration_time):
        'Set touch caret expiration time in milliseconds.'
        self.marionette.execute_script(
            'SpecialPowers.setIntPref("touchcaret.expiration.time", arguments[0]);',
            script_args=[expiration_time])

    def openTestHtml(self, enabled=True, expiration_time=None):
        '''Open html for testing and locate elements, enable/disable touch caret, and
        set touch caret expiration time in milliseconds).

        '''
        self.marionette.execute_script(
            'SpecialPowers.setBoolPref("touchcaret.enabled", %s);' %
            ('true' if enabled else 'false'))

        # Set a larger expiration time to avoid intermittent test failures.
        if expiration_time is not None:
            self.expiration_time = expiration_time

        test_html = self.marionette.absolute_url('test_touchcaret.html')
        self.marionette.navigate(test_html)

        self._input = self.marionette.find_element(*self._input_selector)
        self._textarea = self.marionette.find_element(*self._textarea_selector)
        self._contenteditable = self.marionette.find_element(*self._contenteditable_selector)

    def _test_move_caret_to_the_right_by_one_character(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = sel.content
        target_content = target_content[:1] + content_to_add + target_content[1:]

        # Get touch caret (x, y) at position 1 and 2.
        el.tap()
        sel.move_caret_to_front()
        caret0_x, caret0_y = sel.caret_location()
        touch_caret0_x, touch_caret0_y = sel.touch_caret_location()
        sel.move_caret_by_offset(1)
        touch_caret1_x, touch_caret1_y = sel.touch_caret_location()

        # Tap the front of the input to make touch caret appear.
        el.tap(caret0_x, caret0_y)

        # Move touch caret
        self.actions.flick(el, touch_caret0_x, touch_caret0_y,
                           touch_caret1_x, touch_caret1_y).perform()

        el.send_keys(content_to_add)
        assertFunc(target_content, sel.content)

    def _test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = sel.content + content_to_add

        # Tap the front of the input to make touch caret appear.
        el.tap()
        sel.move_caret_to_front()
        el.tap(*sel.caret_location())

        # Move touch caret to the bottom-right corner of the element.
        src_x, src_y = sel.touch_caret_location()
        dest_x, dest_y = el.size['width'], el.size['height']
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(target_content, sel.content)

    def _test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = content_to_add + sel.content

        # Tap to make touch caret appear. Note: it's strange that when the caret
        # is at the end, the rect of the caret in <textarea> cannot be obtained.
        # A bug perhaps.
        el.tap()
        sel.move_caret_to_end()
        sel.move_caret_by_offset(1, backward=True)
        el.tap(*sel.caret_location())

        # Move touch caret to the top-left corner of the input box.
        src_x, src_y = sel.touch_caret_location()
        dest_x, dest_y = 0, 0
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(target_content, sel.content)

    def _test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content

        # Get touch caret expiration time in millisecond, and convert it to second.
        timeout = self.expiration_time / 1000.0

        # Tap to make touch caret appear. Note: it's strange that when the caret
        # is at the end, the rect of the caret in <textarea> cannot be obtained.
        # A bug perhaps.
        el.tap()
        sel.move_caret_to_end()
        sel.move_caret_by_offset(1, backward=True)
        el.tap(*sel.caret_location())

        # Wait until touch caret disappears, then pretend to move it to the
        # top-left corner of the input box.
        src_x, src_y = sel.touch_caret_location()
        dest_x, dest_y = 0, 0
        self.actions.wait(timeout).flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(non_target_content, sel.content)

    def _test_touch_caret_hides_after_receiving_wheel_event(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content

        # Tap to make touch caret appear. Note: it's strange that when the caret
        # is at the end, the rect of the caret in <textarea> cannot be obtained.
        # A bug perhaps.
        el.tap()
        sel.move_caret_to_end()
        sel.move_caret_by_offset(1, backward=True)
        el.tap(*sel.caret_location())

        # Send an arbitrary scroll-down-10px wheel event to the center of the
        # input box to hide touch caret. Then pretend to move it to the top-left
        # corner of the input box.
        src_x, src_y = sel.touch_caret_location()
        dest_x, dest_y = 0, 0
        el_center_x, el_center_y = el.rect['x'], el.rect['y']
        self.marionette.execute_script(
            '''var utils = SpecialPowers.getDOMWindowUtils(window);
            utils.sendWheelEvent(arguments[0], arguments[1],
                                 0, 10, 0, WheelEvent.DOM_DELTA_PIXEL,
                                 0, 0, 0, 0);''',
            script_args=[el_center_x, el_center_y]
        )
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(non_target_content, sel.content)


    ########################################################################
    # <input> test cases with touch caret enabled
    ########################################################################
    def test_input_move_caret_to_the_right_by_one_character(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_the_right_by_one_character(self._input, self.assertEqual)

    def test_input_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._input, self.assertEqual)

    def test_input_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._input, self.assertEqual)

    def test_input_touch_caret_timeout(self):
        self.openTestHtml(enabled=True)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._input, self.assertNotEqual)

    def test_input_touch_caret_hides_after_receiving_wheel_event(self):
        self.openTestHtml(enabled=True, expiration_time=0)
        self._test_touch_caret_hides_after_receiving_wheel_event(self._input, self.assertNotEqual)

    ########################################################################
    # <input> test cases with touch caret disabled
    ########################################################################
    def test_input_move_caret_to_the_right_by_one_character_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._input, self.assertNotEqual)

    def test_input_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._input, self.assertNotEqual)

    ########################################################################
    # <textarea> test cases with touch caret enabled
    ########################################################################
    def test_textarea_move_caret_to_the_right_by_one_character(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_the_right_by_one_character(self._textarea, self.assertEqual)

    def test_textarea_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._textarea, self.assertEqual)

    def test_textarea_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._textarea, self.assertEqual)

    def test_textarea_touch_caret_timeout(self):
        self.openTestHtml(enabled=True)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._textarea, self.assertNotEqual)

    def test_textarea_touch_caret_hides_after_receiving_wheel_event(self):
        self.openTestHtml(enabled=True, expiration_time=0)
        self._test_touch_caret_hides_after_receiving_wheel_event(self._textarea, self.assertNotEqual)

    ########################################################################
    # <textarea> test cases with touch caret disabled
    ########################################################################
    def test_textarea_move_caret_to_the_right_by_one_character_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._textarea, self.assertNotEqual)

    def test_textarea_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._textarea, self.assertNotEqual)

    ########################################################################
    # <div> contenteditable test cases with touch caret enabled
    ########################################################################
    def test_contenteditable_move_caret_to_the_right_by_one_character(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_the_right_by_one_character(self._contenteditable, self.assertEqual)

    def test_contenteditable_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._contenteditable, self.assertEqual)

    def test_contenteditable_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.openTestHtml(enabled=True, expiration_time=self._large_expiration_time)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._contenteditable, self.assertEqual)

    def test_contenteditable_touch_caret_timeout(self):
        self.openTestHtml(enabled=True)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._contenteditable, self.assertNotEqual)

    def test_contenteditable_touch_caret_hides_after_receiving_wheel_event(self):
        self.openTestHtml(enabled=True, expiration_time=0)
        self._test_touch_caret_hides_after_receiving_wheel_event(self._contenteditable, self.assertNotEqual)

    ########################################################################
    # <div> contenteditable test cases with touch caret disabled
    ########################################################################
    def test_contenteditable_move_caret_to_the_right_by_one_character_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._contenteditable, self.assertNotEqual)

    def test_contenteditable_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.openTestHtml(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self._contenteditable, self.assertNotEqual)
