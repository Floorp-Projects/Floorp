# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette_driver.selection import SelectionManager
from marionette import MarionetteTestCase
import string


class CommonCaretTestCase(object):
    '''Common test cases for a collapsed selection with a single caret.

    To run these test cases, a subclass must inherit from both this class and
    MarionetteTestCase.

    '''
    _input_selector = (By.ID, 'input')
    _textarea_selector = (By.ID, 'textarea')
    _contenteditable_selector = (By.ID, 'contenteditable')

    def setUp(self):
        # Code to execute before a test is being run.
        super(CommonCaretTestCase, self).setUp()
        self.actions = Actions(self.marionette)

        # The caret to be tested.
        self.caret_tested_pref = None

        # The caret to be disabled in this test suite.
        self.caret_disabled_pref = None
        self.caret_timeout_ms_pref = None

    def set_pref(self, pref_name, value):
        '''Set a preference to value.

        For example:
        >>> set_pref('layout.accessiblecaret.enabled', True)

        '''
        pref_name = repr(pref_name)
        if isinstance(value, bool):
            value = 'true' if value else 'false'
            func = 'setBoolPref'
        elif isinstance(value, int):
            value = str(value)
            func = 'setIntPref'
        else:
            value = repr(value)
            func = 'setCharPref'

        with self.marionette.using_context('chrome'):
            script = 'Services.prefs.%s(%s, %s)' % (func, pref_name, value)
            self.marionette.execute_script(script)

    def timeout_ms(self):
        'Return touch caret expiration time in milliseconds.'
        with self.marionette.using_context('chrome'):
            return self.marionette.execute_script(
                'return Services.prefs.getIntPref("%s");' % self.caret_timeout_ms_pref)

    def open_test_html(self, enabled=True, timeout_ms=0):
        '''Open html for testing and locate elements, enable/disable touch caret, and
        set touch caret expiration time in milliseconds).

        '''
        self.set_pref(self.caret_tested_pref, enabled)
        self.set_pref(self.caret_disabled_pref, False)
        self.set_pref(self.caret_timeout_ms_pref, timeout_ms)

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

    def _test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = content_to_add + sel.content

        # Get touch caret location at the front.
        el.tap()
        sel.move_caret_to_front()
        dest_x, dest_y = sel.touch_caret_location()

        # Tap to make touch caret appear. Note: it's strange that when the caret
        # is at the end, the rect of the caret in <textarea> cannot be obtained.
        # A bug perhaps.
        el.tap()
        sel.move_caret_to_end()
        sel.move_caret_by_offset(1, backward=True)
        el.tap(*sel.caret_location())
        src_x, src_y = sel.touch_caret_location()

        # Move touch caret to the front of the input box.
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(target_content, sel.content)

    def _test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self, el, assertFunc):
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content

        # Get touch caret expiration time in millisecond, and convert it to second.
        timeout = self.timeout_ms() / 1000.0

        # Set a 3x timeout margin to prevent intermittent test failures.
        timeout *= 3

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
            '''
            var utils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIDOMWindowUtils);
            utils.sendWheelEvent(arguments[0], arguments[1],
                                 0, 10, 0, WheelEvent.DOM_DELTA_PIXEL,
                                 0, 0, 0, 0);
            ''',
            script_args=[el_center_x, el_center_y],
            sandbox='system'
        )
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        el.send_keys(content_to_add)
        assertFunc(non_target_content, sel.content)

    def _test_caret_not_appear_when_typing_in_scrollable_content(self, el, assertFunc):
        sel = SelectionManager(el)

        content_to_add = '!'
        target_content = sel.content + string.ascii_letters + content_to_add

        el.tap()
        sel.move_caret_to_end()

        # Insert a long string to the end of the <input>, which triggers
        # ScrollPositionChanged event.
        el.send_keys(string.ascii_letters)

        # The caret should not be visible. If it does appear wrongly due to the
        # ScrollPositionChanged event, we can drag it to the front of the
        # <input> to change the cursor position.
        src_x, src_y = sel.touch_caret_location()
        dest_x, dest_y = 0, 0
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        # The content should be inserted at the end of the <input>.
        el.send_keys(content_to_add)

        assertFunc(target_content, sel.content)

    ########################################################################
    # <input> test cases with touch caret enabled
    ########################################################################
    def test_input_move_caret_to_the_right_by_one_character(self):
        self.open_test_html()
        self._test_move_caret_to_the_right_by_one_character(self._input, self.assertEqual)

    def test_input_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.open_test_html()
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._input, self.assertEqual)

    def test_input_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.open_test_html()
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._input, self.assertEqual)

    def test_input_caret_not_appear_when_typing_in_scrollable_content(self):
        self.open_test_html()
        self._test_caret_not_appear_when_typing_in_scrollable_content(self._input, self.assertEqual)

    def test_input_touch_caret_timeout(self):
        self.open_test_html(timeout_ms=1000)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._input, self.assertNotEqual)

    ########################################################################
    # <input> test cases with touch caret disabled
    ########################################################################
    def test_input_move_caret_to_the_right_by_one_character_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._input, self.assertNotEqual)

    def test_input_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._input, self.assertNotEqual)

    ########################################################################
    # <textarea> test cases with touch caret enabled
    ########################################################################
    def test_textarea_move_caret_to_the_right_by_one_character(self):
        self.open_test_html()
        self._test_move_caret_to_the_right_by_one_character(self._textarea, self.assertEqual)

    def test_textarea_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.open_test_html()
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._textarea, self.assertEqual)

    def test_textarea_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.open_test_html()
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._textarea, self.assertEqual)

    def test_textarea_touch_caret_timeout(self):
        self.open_test_html(timeout_ms=1000)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._textarea, self.assertNotEqual)

    ########################################################################
    # <textarea> test cases with touch caret disabled
    ########################################################################
    def test_textarea_move_caret_to_the_right_by_one_character_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._textarea, self.assertNotEqual)

    def test_textarea_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._textarea, self.assertNotEqual)

    ########################################################################
    # <div> contenteditable test cases with touch caret enabled
    ########################################################################
    def test_contenteditable_move_caret_to_the_right_by_one_character(self):
        self.open_test_html()
        self._test_move_caret_to_the_right_by_one_character(self._contenteditable, self.assertEqual)

    def test_contenteditable_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self):
        self.open_test_html()
        self._test_move_caret_to_end_by_dragging_touch_caret_to_bottom_right_corner(self._contenteditable, self.assertEqual)

    def test_contenteditable_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner(self):
        self.open_test_html()
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._contenteditable, self.assertEqual)

    def test_contenteditable_touch_caret_timeout(self):
        self.open_test_html(timeout_ms=1000)
        self._test_touch_caret_timeout_by_dragging_it_to_top_left_corner_after_timout(self._contenteditable, self.assertNotEqual)

    ########################################################################
    # <div> contenteditable test cases with touch caret disabled
    ########################################################################
    def test_contenteditable_move_caret_to_the_right_by_one_character_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_the_right_by_one_character(self._contenteditable, self.assertNotEqual)

    def test_contenteditable_move_caret_to_front_by_dragging_touch_caret_to_top_left_corner_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_caret_to_front_by_dragging_touch_caret_to_front_of_content(self._contenteditable, self.assertNotEqual)


class TouchCaretTestCase(CommonCaretTestCase, MarionetteTestCase):
    def setUp(self):
        super(TouchCaretTestCase, self).setUp()
        self.caret_tested_pref = 'touchcaret.enabled'
        self.caret_disabled_pref = 'layout.accessiblecaret.enabled'
        self.caret_timeout_ms_pref = 'touchcaret.expiration.time'

    def test_input_touch_caret_hides_after_receiving_wheel_event(self):
        self.open_test_html()
        self._test_touch_caret_hides_after_receiving_wheel_event(self._input, self.assertNotEqual)

    def test_textarea_touch_caret_hides_after_receiving_wheel_event(self):
        self.open_test_html()
        self._test_touch_caret_hides_after_receiving_wheel_event(self._textarea, self.assertNotEqual)

    def test_contenteditable_touch_caret_hides_after_receiving_wheel_event(self):
        self.open_test_html()
        self._test_touch_caret_hides_after_receiving_wheel_event(self._contenteditable, self.assertNotEqual)


class AccessibleCaretCursorModeTestCase(CommonCaretTestCase, MarionetteTestCase):
    def setUp(self):
        super(AccessibleCaretCursorModeTestCase, self).setUp()
        self.caret_tested_pref = 'layout.accessiblecaret.enabled'
        self.caret_disabled_pref = 'touchcaret.enabled'
        self.caret_timeout_ms_pref = 'layout.accessiblecaret.timeout_ms'
