# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import string
import sys
import os

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from selection import (
    CaretActions,
    SelectionManager,
)
from marionette_driver.by import By
from marionette_harness.marionette_test import (
    MarionetteTestCase,
    parameterized,
)


class AccessibleCaretCursorModeTestCase(MarionetteTestCase):
    '''Test cases for AccessibleCaret under cursor mode.

    We call the blinking cursor (nsCaret) as cursor, and call AccessibleCaret as
    caret for short.

    '''
    # Element IDs.
    _input_id = 'input'
    _input_padding_id = 'input-padding'
    _textarea_id = 'textarea'
    _textarea_one_line_id = 'textarea-one-line'
    _contenteditable_id = 'contenteditable'

    # Test html files.
    _cursor_html = 'layout/test_carets_cursor.html'

    def setUp(self):
        # Code to execute before every test is running.
        super(AccessibleCaretCursorModeTestCase, self).setUp()
        self.caret_tested_pref = 'layout.accessiblecaret.enabled'
        self.hide_carets_for_mouse = 'layout.accessiblecaret.hide_carets_for_mouse_input'
        self.prefs = {
            self.caret_tested_pref: True,
            self.hide_carets_for_mouse: False,
        }
        self.marionette.set_prefs(self.prefs)
        self.actions = CaretActions(self.marionette)

    def tearDown(self):
        self.marionette.actions.release()
        super(AccessibleCaretCursorModeTestCase, self).tearDown()

    def open_test_html(self, test_html):
        self.marionette.navigate(self.marionette.absolute_url(test_html))

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_the_right_by_one_character(self, el_id):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = sel.content
        target_content = target_content[:1] + content_to_add + target_content[1:]

        # Get first caret (x, y) at position 1 and 2.
        el.tap()
        sel.move_cursor_to_front()
        cursor0_x, cursor0_y = sel.cursor_location()
        first_caret0_x, first_caret0_y = sel.first_caret_location()
        sel.move_cursor_by_offset(1)
        first_caret1_x, first_caret1_y = sel.first_caret_location()

        # Tap the front of the input to make first caret appear.
        el.tap(cursor0_x, cursor0_y)

        # Move first caret.
        self.actions.flick(el, first_caret0_x, first_caret0_y,
                           first_caret1_x, first_caret1_y).perform()

        self.actions.send_keys(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_end_by_dragging_caret_to_bottom_right_corner(self, el_id):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = sel.content + content_to_add

        # Tap the front of the input to make first caret appear.
        el.tap()
        sel.move_cursor_to_front()
        el.tap(*sel.cursor_location())

        # Move first caret to the bottom-right corner of the element.
        src_x, src_y = sel.first_caret_location()
        dest_x, dest_y = el.rect['width'], el.rect['height']
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        self.actions.send_keys(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_move_cursor_to_front_by_dragging_caret_to_front(self, el_id):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = content_to_add + sel.content

        # Get first caret location at the front.
        el.tap()
        sel.move_cursor_to_front()
        dest_x, dest_y = sel.first_caret_location()

        # Tap to make first caret appear. Note: it's strange that when the caret
        # is at the end, the rect of the caret in <textarea> cannot be obtained.
        # A bug perhaps.
        el.tap()
        sel.move_cursor_to_end()
        sel.move_cursor_by_offset(1, backward=True)
        el.tap(*sel.cursor_location())
        src_x, src_y = sel.first_caret_location()

        # Move first caret to the front of the input box.
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        self.actions.send_keys(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    def test_caret_not_appear_when_typing_in_scrollable_content(self):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, self._input_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content + string.ascii_letters

        el.tap()
        sel.move_cursor_to_end()

        # Insert a long string to the end of the <input>, which triggers
        # ScrollPositionChanged event.
        el.send_keys(string.ascii_letters)

        # The caret should not be visible. If it does appear wrongly due to the
        # ScrollPositionChanged event, we can drag it to the front of the
        # <input> to change the cursor position.
        src_x, src_y = sel.first_caret_location()
        dest_x, dest_y = 0, 0
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        # The content should not be inserted at the front of the <input>.
        el.send_keys(content_to_add)

        self.assertNotEqual(non_target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_input_padding_id, el_id=_input_padding_id)
    @parameterized(_textarea_one_line_id, el_id=_textarea_one_line_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_caret_not_jump_when_dragging_to_editable_content_boundary(self, el_id):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = sel.content + content_to_add

        # Goal: the cursor position is not changed after dragging the caret down
        # on the Y-axis.
        el.tap()
        sel.move_cursor_to_front()
        el.tap(*sel.cursor_location())
        x, y = sel.first_caret_location()

        # Drag the caret down by 50px, and insert '!'.
        self.actions.flick(el, x, y, x, y + 50).perform()
        self.actions.send_keys(content_to_add).perform()
        self.assertNotEqual(non_target_content, sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_input_padding_id, el_id=_input_padding_id)
    @parameterized(_textarea_one_line_id, el_id=_textarea_one_line_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_caret_not_jump_to_front_when_dragging_up_to_editable_content_boundary(self, el_id):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        content_to_add = '!'
        non_target_content = content_to_add + sel.content

        # Goal: the cursor position is not changed after dragging the caret down
        # on the Y-axis.
        el.tap()
        sel.move_cursor_to_end()
        sel.move_cursor_by_offset(1, backward=True)
        el.tap(*sel.cursor_location())
        x, y = sel.first_caret_location()

        # Drag the caret up by 40px, and insert '!'.
        self.actions.flick(el, x, y, x, y - 40).perform()
        self.actions.send_keys(content_to_add).perform()
        self.assertNotEqual(non_target_content, sel.content)

    def test_drag_caret_from_front_to_end_across_columns(self):
        self.open_test_html('layout/test_carets_columns.html')
        el = self.marionette.find_element(By.ID, 'columns-inner')
        sel = SelectionManager(el)
        content_to_add = '!'
        target_content = sel.content + content_to_add

        # Goal: the cursor position can be changed by dragging the caret from
        # the front to the end of the content.

        # Tap to make the cursor appear.
        before_image_1 = self.marionette.find_element(By.ID, 'before-image-1')
        before_image_1.tap()

        # Tap the front of the content to make first caret appear.
        sel.move_cursor_to_front()
        el.tap(*sel.cursor_location())
        src_x, src_y = sel.first_caret_location()
        dest_x, dest_y = el.rect['width'], el.rect['height']

        # Drag the first caret to the bottom-right corner of the element.
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        self.actions.send_keys(content_to_add).perform()
        self.assertEqual(target_content, sel.content)

    def test_move_cursor_to_front_by_dragging_caret_to_front_br_element(self):
        self.open_test_html(self._cursor_html)
        el = self.marionette.find_element(By.ID, self._contenteditable_id)
        sel = SelectionManager(el)
        content_to_add_1 = '!'
        content_to_add_2 = '\n\n'
        target_content = content_to_add_1 + content_to_add_2 + sel.content

        # Goal: the cursor position can be changed by dragging the caret from
        # the end of the content to the front br element. Because we cannot get
        # caret location if it's on a br element, we need to get the first caret
        # location then adding the new lines.

        # Get first caret location at the front.
        el.tap()
        sel.move_cursor_to_front()
        dest_x, dest_y = sel.first_caret_location()

        # Append new line to the front of the content.
        el.send_keys(content_to_add_2);

        # Tap to make first caret appear.
        el.tap()
        sel.move_cursor_to_end()
        sel.move_cursor_by_offset(1, backward=True)
        el.tap(*sel.cursor_location())
        src_x, src_y = sel.first_caret_location()

        # Move first caret to the front of the input box.
        self.actions.flick(el, src_x, src_y, dest_x, dest_y).perform()

        self.actions.send_keys(content_to_add_1).perform()
        self.assertEqual(target_content, sel.content)
