# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import string

from marionette.marionette_test import (
    MarionetteTestCase, parameterized
)
from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette_driver.selection import SelectionManager


class AccessibleCaretCursorModeTestCase(MarionetteTestCase):
    '''Test cases for AccessibleCaret under cursor mode.

    We call the blinking cursor (nsCaret) as cursor, and call AccessibleCaret as
    caret for short.

    '''
    _input_id = 'input'
    _textarea_id = 'textarea'
    _contenteditable_id = 'contenteditable'

    def setUp(self):
        # Code to execute before every test is running.
        super(AccessibleCaretCursorModeTestCase, self).setUp()
        self.caret_tested_pref = 'layout.accessiblecaret.enabled'
        self.caret_timeout_ms_pref = 'layout.accessiblecaret.timeout_ms'
        self.prefs = {
            self.caret_tested_pref: True,
            self.caret_timeout_ms_pref: 0,
        }
        self.marionette.set_prefs(self.prefs)
        self.actions = Actions(self.marionette)

    def open_test_html(self):
        test_html = self.marionette.absolute_url('test_touchcaret.html')
        self.marionette.navigate(test_html)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_the_right_by_one_character(self, el_id):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, el_id)
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

        self.actions.key_down(content_to_add).key_up(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_end_by_dragging_caret_to_bottom_right_corner(self, el_id):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, el_id)
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

        self.actions.key_down(content_to_add).key_up(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_front_by_dragging_caret_to_front(self, el_id):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, el_id)
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

        self.actions.key_down(content_to_add).key_up(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_dragging_caret_to_top_left_corner_after_timeout(self, el_id):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content

        # Set caret timeout to be 1 second.
        timeout = 1
        self.marionette.set_pref(self.caret_timeout_ms_pref, timeout * 1000)

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

        self.actions.key_down(content_to_add).key_up(content_to_add).perform()
        self.assertNotEqual(non_target_content, sel.content)

    def test_caret_not_appear_when_typing_in_scrollable_content(self):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, self._input_id)
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

        self.assertEqual(target_content, sel.content)

    def test_caret_not_jump_when_dragging_to_editable_content_boundary(self):
        self.open_test_html()
        el = self.marionette.find_element(By.ID, self._input_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = sel.content + content_to_add

        # Goal: the cursor position does not being changed after dragging the
        # caret down on the Y-axis.
        el.tap()
        sel.move_caret_to_front()
        el.tap(*sel.caret_location())
        x, y = sel.touch_caret_location()

        # Drag the caret down by 50px, and insert '!'.
        self.actions.flick(el, x, y, x, y + 50).perform()
        self.actions.key_down(content_to_add).key_up(content_to_add).perform()
        self.assertNotEqual(non_target_content, sel.content)
