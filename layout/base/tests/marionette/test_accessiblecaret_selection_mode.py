# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette_driver.selection import SelectionManager
from marionette_harness.marionette_test import (
    MarionetteTestCase,
    SkipTest,
    parameterized
)


def skip_if_not_rotatable(target):
    def wrapper(self, *args, **kwargs):
        if not self.marionette.session_capabilities.get('rotatable'):
            raise SkipTest('skipping due to device not rotatable')
        return target(self, *args, **kwargs)
    return wrapper


class AccessibleCaretSelectionModeTestCase(MarionetteTestCase):
    '''Test cases for AccessibleCaret under selection mode.'''
    # Element IDs.
    _input_id = 'input'
    _input_padding_id = 'input-padding'
    _textarea_id = 'textarea'
    _textarea2_id = 'textarea2'
    _textarea_one_line_id = 'textarea-one-line'
    _textarea_rtl_id = 'textarea-rtl'
    _contenteditable_id = 'contenteditable'
    _contenteditable2_id = 'contenteditable2'
    _content_id = 'content'
    _content2_id = 'content2'
    _non_selectable_id = 'non-selectable'

    # Test html files.
    _selection_html = 'test_carets_selection.html'
    _multipleline_html = 'test_carets_multipleline.html'
    _multiplerange_html = 'test_carets_multiplerange.html'
    _longtext_html = 'test_carets_longtext.html'
    _iframe_html = 'test_carets_iframe.html'
    _display_none_html = 'test_carets_display_none.html'

    def setUp(self):
        # Code to execute before every test is running.
        super(AccessibleCaretSelectionModeTestCase, self).setUp()
        self.carets_tested_pref = 'layout.accessiblecaret.enabled'
        self.prefs = {
            'layout.word_select.eat_space_to_next_word': False,
            self.carets_tested_pref: True,
        }
        self.marionette.set_prefs(self.prefs)
        self.actions = Actions(self.marionette)

    def open_test_html(self, test_html):
        self.marionette.navigate(self.marionette.absolute_url(test_html))

    def word_offset(self, text, ordinal):
        'Get the character offset of the ordinal-th word in text.'
        tokens = re.split(r'(\S+)', text)         # both words and spaces
        spaces = tokens[0::2]                     # collect spaces at odd indices
        words = tokens[1::2]                      # collect word at even indices

        if ordinal >= len(words):
            raise IndexError('Only %d words in text, but got ordinal %d' %
                             (len(words), ordinal))

        # Cursor position of the targeting word is behind the the first
        # character in the word. For example, offset to 'def' in 'abc def' is
        # between 'd' and 'e'.
        offset = len(spaces[0]) + 1
        offset += sum(len(words[i]) + len(spaces[i + 1]) for i in range(ordinal))
        return offset

    def test_word_offset(self):
        text = ' ' * 3 + 'abc' + ' ' * 3 + 'def'

        self.assertTrue(self.word_offset(text, 0), 4)
        self.assertTrue(self.word_offset(text, 1), 10)
        with self.assertRaises(IndexError):
            self.word_offset(text, 2)

    def word_location(self, el, ordinal):
        '''Get the location (x, y) of the ordinal-th word in el.

        The ordinal starts from 0.

        Note: this function has a side effect which changes focus to the
        target element el.

        '''
        sel = SelectionManager(el)
        offset = self.word_offset(sel.content, ordinal)

        # Move the blinking cursor to the word.
        el.tap()
        sel.move_cursor_to_front()
        sel.move_cursor_by_offset(offset)
        x, y = sel.cursor_location()

        return x, y

    def rect_relative_to_window(self, el):
        '''Get element's bounding rectangle.

        This function is similar to el.rect, but the coordinate is relative to
        the top left corner of the window instead of the document.

        '''
        return self.marionette.execute_script('''
            let rect = arguments[0].getBoundingClientRect();
            return {x: rect.x, y:rect.y, width: rect.width, height: rect.height};
            ''', script_args=[el])

    def long_press_on_location(self, el, x=None, y=None):
        '''Long press the location (x, y) to select a word.

        If no (x, y) are given, it will be targeted at the center of the
        element. On Windows, those spaces after the word will also be selected.
        This function sends synthesized eMouseLongTap to gecko.

        '''
        rect = self.rect_relative_to_window(el)
        target_x = rect['x'] + (x if x is not None else rect['width'] // 2)
        target_y = rect['y'] + (y if y is not None else rect['height'] // 2)

        self.marionette.execute_script('''
            let Ci = Components.interfaces;
            let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
            utils.sendTouchEventToWindow('touchstart', [0],
                                         [arguments[0]], [arguments[1]],
                                         [1], [1], [0], [1], 1, 0);
            utils.sendMouseEventToWindow('mouselongtap', arguments[0], arguments[1],
                                          0, 1, 0);
            utils.sendTouchEventToWindow('touchend', [0],
                                         [arguments[0]], [arguments[1]],
                                         [1], [1], [0], [1], 1, 0);
            ''', script_args=[target_x, target_y], sandbox='system')

    def long_press_on_word(self, el, wordOrdinal):
        x, y = self.word_location(el, wordOrdinal)
        self.long_press_on_location(el, x, y)

    def to_unix_line_ending(self, s):
        """Changes all Windows/Mac line endings in s to UNIX line endings."""

        return s.replace('\r\n', '\n').replace('\r', '\n')

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_long_press_to_select_a_word(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        self._test_long_press_to_select_a_word(el)

    def _test_long_press_to_select_a_word(self, el):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 2, 'Expect at least two words in the content.')
        target_content = words[0]

        # Goal: Select the first word.
        self.long_press_on_word(el, 0)

        # Ignore extra spaces selected after the word.
        self.assertEqual(target_content, sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_drag_carets(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Goal: Select all text after the first word.
        target_content = original_content[len(words[0]):]

        # Get the location of the carets at the end of the content for later
        # use.
        el.tap()
        sel.select_all()
        end_caret_x, end_caret_y = sel.second_caret_location()

        self.long_press_on_word(el, 0)

        # Drag the second caret to the end of the content.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, end_caret_x, end_caret_y).perform()

        # Drag the first caret to the previous position of the second caret.
        self.actions.flick(el, caret1_x, caret1_y, caret2_x, caret2_y).perform()

        self.assertEqual(target_content, sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_drag_swappable_carets(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        target_content1 = words[0]
        target_content2 = original_content[len(words[0]):]

        # Get the location of the carets at the end of the content for later
        # use.
        el.tap()
        sel.select_all()
        end_caret_x, end_caret_y = sel.second_caret_location()

        self.long_press_on_word(el, 0)

        # Drag the first caret to the end and back to where it was
        # immediately. The selection range should not be collapsed.
        caret1_x, caret1_y = sel.first_caret_location()
        self.actions.flick(el, caret1_x, caret1_y, end_caret_x, end_caret_y)\
                    .flick(el, end_caret_x, end_caret_y, caret1_x, caret1_y).perform()
        self.assertEqual(target_content1, sel.selected_content)

        # Drag the first caret to the end.
        caret1_x, caret1_y = sel.first_caret_location()
        self.actions.flick(el, caret1_x, caret1_y, end_caret_x, end_caret_y).perform()
        self.assertEqual(target_content2, sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_minimum_select_one_character(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        self._test_minimum_select_one_character(el)

    @parameterized(_textarea2_id, el_id=_textarea2_id)
    @parameterized(_contenteditable2_id, el_id=_contenteditable2_id)
    @parameterized(_content2_id, el_id=_content2_id)
    def test_minimum_select_one_character2(self, el_id):
        self.open_test_html(self._multipleline_html)
        el = self.marionette.find_element(By.ID, el_id)
        self._test_minimum_select_one_character(el)

    def _test_minimum_select_one_character(self, el, x=None, y=None):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Get the location of the carets at the end of the content for later
        # use.
        sel.select_all()
        end_caret_x, end_caret_y = sel.second_caret_location()
        el.tap()

        # Goal: Select the first character.
        target_content = original_content[0]

        if x and y:
            # If we got x and y from the arguments, use it as a hint of the
            # location of the first word
            pass
        else:
            x, y = self.word_location(el, 0)
        self.long_press_on_location(el, x, y)

        # Drag the second caret to the end of the content.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, end_caret_x, end_caret_y).perform()

        # Drag the second caret to the position of the first caret.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, caret1_x, caret1_y).perform()

        self.assertEqual(target_content, sel.selected_content)

    @parameterized(_input_id + '_to_' + _textarea_id,
                   el1_id=_input_id, el2_id=_textarea_id)
    @parameterized(_input_id + '_to_' + _contenteditable_id,
                   el1_id=_input_id, el2_id=_contenteditable_id)
    @parameterized(_input_id + '_to_' + _content_id,
                   el1_id=_input_id, el2_id=_content_id)
    @parameterized(_textarea_id + '_to_' + _input_id,
                   el1_id=_textarea_id, el2_id=_input_id)
    @parameterized(_textarea_id + '_to_' + _contenteditable_id,
                   el1_id=_textarea_id, el2_id=_contenteditable_id)
    @parameterized(_textarea_id + '_to_' + _content_id,
                   el1_id=_textarea_id, el2_id=_content_id)
    @parameterized(_contenteditable_id + '_to_' + _input_id,
                   el1_id=_contenteditable_id, el2_id=_input_id)
    @parameterized(_contenteditable_id + '_to_' + _textarea_id,
                   el1_id=_contenteditable_id, el2_id=_textarea_id)
    @parameterized(_contenteditable_id + '_to_' + _content_id,
                   el1_id=_contenteditable_id, el2_id=_content_id)
    @parameterized(_content_id + '_to_' + _input_id,
                   el1_id=_content_id, el2_id=_input_id)
    @parameterized(_content_id + '_to_' + _textarea_id,
                   el1_id=_content_id, el2_id=_textarea_id)
    @parameterized(_content_id + '_to_' + _contenteditable_id,
                   el1_id=_content_id, el2_id=_contenteditable_id)
    def test_long_press_changes_focus_from(self, el1_id, el2_id):
        '''Test the focus could be changed from el1 to el2 by long press.

        If the focus is changed to e2 successfully, the carets should appear and
        could be dragged.

        '''
        # Goal: Tap to focus el1, and then select the first character on
        # el2.

        # We want to collect the location of the first word in el2 here
        # since self.word_location() has the side effect which would
        # change the focus.
        self.open_test_html(self._selection_html)
        el1 = self.marionette.find_element(By.ID, el1_id)
        el2 = self.marionette.find_element(By.ID, el2_id)
        x, y = self.word_location(el2, 0)
        el1.tap()
        self._test_minimum_select_one_character(el2, x=x, y=y)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_focus_not_changed_by_long_press_on_non_selectable(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        non_selectable = self.marionette.find_element(By.ID, self._non_selectable_id)

        # Goal: Focus remains on the editable element el after long pressing on
        # the non-selectable element.
        sel = SelectionManager(el)
        self.long_press_on_word(el, 0)
        self.long_press_on_location(non_selectable)
        active_sel = SelectionManager(self.marionette.get_active_element())
        self.assertEqual(sel.content, active_sel.content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_handle_tilt_when_carets_overlap_each_other(self, el_id):
        '''Test tilt handling when carets overlap to each other.

        Let the two carets overlap each other. If they are set to tilted
        successfully, tapping the tilted carets should not cause the selection
        to be collapsed and the carets should be draggable.

        '''
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Goal: Select the first word.
        self.long_press_on_word(el, 0)
        target_content = sel.selected_content

        # Drag the first caret to the position of the second caret to trigger
        # carets overlapping.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret1_x, caret1_y, caret2_x, caret2_y).perform()

        # We make two hit tests targeting the left edge of the left tilted caret
        # and the right edge of the right tilted caret. If either of the hits is
        # missed, selection would be collapsed and both carets should not be
        # draggable.
        (caret3_x, caret3_y), (caret4_x, caret4_y) = sel.carets_location()

        # The following values are from ua.css and all.js
        caret_width = float(self.marionette.get_pref('layout.accessiblecaret.width'))
        caret_margin_left = float(self.marionette.get_pref('layout.accessiblecaret.margin-left'))
        tilt_right_margin_left = 0.41 * caret_width
        tilt_left_margin_left = -0.39 * caret_width

        left_caret_left_edge_x = caret3_x + caret_margin_left + tilt_left_margin_left
        el.tap(left_caret_left_edge_x + 2, caret3_y)

        right_caret_right_edge_x = (caret4_x + caret_margin_left +
                                    tilt_right_margin_left + caret_width)
        el.tap(right_caret_right_edge_x - 2, caret4_y)

        # Drag the first caret back to the initial selection, the first word.
        self.actions.flick(el, caret3_x, caret3_y, caret1_x, caret1_y).perform()

        self.assertEqual(target_content, sel.selected_content)

    def test_drag_caret_over_non_selectable_field(self):
        '''Test dragging the caret over a non-selectable field.

        The selected content should exclude non-selectable elements and the
        second caret should appear in last range's position.

        '''
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, 'bd')
        sel3 = self.marionette.find_element(By.ID, 'sel3')
        sel4 = self.marionette.find_element(By.ID, 'sel4')
        sel6 = self.marionette.find_element(By.ID, 'sel6')

        # Select target element and get target caret location
        self.long_press_on_word(sel4, 3)
        sel = SelectionManager(body)
        end_caret_x, end_caret_y = sel.second_caret_location()

        self.long_press_on_word(sel6, 0)
        end_caret2_x, end_caret2_y = sel.second_caret_location()

        # Select start element
        self.long_press_on_word(sel3, 3)

        # Drag end caret to target location
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(body, caret2_x, caret2_y, end_caret_x, end_caret_y, 1).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()),
                         'this 3\nuser can select this')

        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(body, caret2_x, caret2_y, end_caret2_x, end_caret2_y, 1).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()),
                         'this 3\nuser can select this 4\nuser can select this 5\nuser')

        # Drag first caret to target location
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(body, caret1_x, caret1_y, end_caret_x, end_caret_y, 1).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()),
                         '4\nuser can select this 5\nuser')

    def test_drag_swappable_caret_over_non_selectable_field(self):
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, 'bd')
        sel3 = self.marionette.find_element(By.ID, 'sel3')
        sel4 = self.marionette.find_element(By.ID, 'sel4')
        sel = SelectionManager(body)

        self.long_press_on_word(sel4, 3)
        (end_caret1_x, end_caret1_y), (end_caret2_x, end_caret2_y) = sel.carets_location()

        self.long_press_on_word(sel3, 3)
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()

        # Drag the first caret down, which will across the second caret.
        self.actions.flick(body, caret1_x, caret1_y, end_caret1_x, end_caret1_y).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()),
                         '3\nuser can select')

        # The old second caret becomes the first caret. Drag it down again.
        self.actions.flick(body, caret2_x, caret2_y, end_caret2_x, end_caret2_y).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()),
                         'this')

    def test_drag_caret_to_beginning_of_a_line(self):
        '''Bug 1094056
        Test caret visibility when caret is dragged to beginning of a line
        '''
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, 'bd')
        sel1 = self.marionette.find_element(By.ID, 'sel1')
        sel2 = self.marionette.find_element(By.ID, 'sel2')

        # Select the first word in the second line
        self.long_press_on_word(sel2, 0)
        sel = SelectionManager(body)
        (start_caret_x, start_caret_y), (end_caret_x, end_caret_y) = sel.carets_location()

        # Select target word in the first line
        self.long_press_on_word(sel1, 2)

        # Drag end caret to the beginning of the second line
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(body, caret2_x, caret2_y, start_caret_x, start_caret_y).perform()

        # Drag end caret back to the target word
        self.actions.flick(body, start_caret_x, start_caret_y, caret2_x, caret2_y).perform()

        self.assertEqual(self.to_unix_line_ending(sel.selected_content), 'select')

    @skip_if_not_rotatable
    def test_caret_position_after_changing_orientation_of_device(self):
        '''Bug 1094072
        If positions of carets are updated correctly, they should be draggable.
        '''
        self.open_test_html(self._longtext_html)
        body = self.marionette.find_element(By.ID, 'bd')
        longtext = self.marionette.find_element(By.ID, 'longtext')

        # Select word in portrait mode, then change to landscape mode
        self.marionette.set_orientation('portrait')
        self.long_press_on_word(longtext, 12)
        sel = SelectionManager(body)
        (p_start_caret_x, p_start_caret_y), (p_end_caret_x, p_end_caret_y) = sel.carets_location()
        self.marionette.set_orientation('landscape')
        (l_start_caret_x, l_start_caret_y), (l_end_caret_x, l_end_caret_y) = sel.carets_location()

        # Drag end caret to the start caret to change the selected content
        self.actions.flick(body, l_end_caret_x, l_end_caret_y,
                           l_start_caret_x, l_start_caret_y).perform()

        # Change orientation back to portrait mode to prevent affecting
        # other tests
        self.marionette.set_orientation('portrait')

        self.assertEqual(self.to_unix_line_ending(sel.selected_content), 'o')

    def test_select_word_inside_an_iframe(self):
        '''Bug 1088552
        The scroll offset in iframe should be taken into consideration properly.
        In this test, we scroll content in the iframe to the bottom to cause a
        huge offset. If we use the right coordinate system, selection should
        work. Otherwise, it would be hard to trigger select word.
        '''
        self.open_test_html(self._iframe_html)
        iframe = self.marionette.find_element(By.ID, 'frame')

        # switch to inner iframe and scroll to the bottom
        self.marionette.switch_to_frame(iframe)
        self.marionette.execute_script(
            'document.getElementById("bd").scrollTop += 999')

        # long press to select bottom text
        body = self.marionette.find_element(By.ID, 'bd')
        sel = SelectionManager(body)
        self._bottomtext = self.marionette.find_element(By.ID, 'bottomtext')
        self.long_press_on_location(self._bottomtext)

        self.assertNotEqual(self.to_unix_line_ending(sel.selected_content), '')

    def test_carets_initialized_in_display_none(self):
        '''Test AccessibleCaretEventHub is properly initialized on a <html> with
        display: none.

        '''
        self.open_test_html(self._display_none_html)
        html = self.marionette.find_element(By.ID, 'html')
        content = self.marionette.find_element(By.ID, 'content')

        # Remove 'display: none' from <html>
        self.marionette.execute_script(
            'arguments[0].style.display = "unset";',
            script_args=[html]
        )

        # If AccessibleCaretEventHub is initialized successfully, select a word
        # should work.
        self._test_long_press_to_select_a_word(content)

    def test_long_press_to_select_when_partial_visible_word_is_selected(self):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, self._input_id)
        sel = SelectionManager(el)

        # To successfully select the second word while the first word is being
        # selected, use sufficient spaces between 'a' and 'b' to avoid the
        # second caret covers on the second word.
        original_content = 'aaaaaaaa          bbbbbbbb'
        el.clear()
        el.send_keys(original_content)
        words = original_content.split()

        # We cannot use self.long_press_on_word() directly since it has will
        # change the cursor position which affects this test. We have to store
        # the position of word 0 and word 1 before long-pressing to select the
        # word.
        word0_x, word0_y = self.word_location(el, 0)
        word1_x, word1_y = self.word_location(el, 1)

        self.long_press_on_location(el, word0_x, word0_y)
        self.assertEqual(words[0], sel.selected_content)

        self.long_press_on_location(el, word1_x, word1_y)
        self.assertEqual(words[1], sel.selected_content)

        self.long_press_on_location(el, word0_x, word0_y)
        self.assertEqual(words[0], sel.selected_content)

        # If the second carets is visible, it can be dragged to the position of
        # the first caret. After that, selection will contain only the first
        # character.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, caret1_x, caret1_y).perform()
        self.assertEqual(words[0][0], sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_input_padding_id, el_id=_input_padding_id)
    @parameterized(_textarea_one_line_id, el_id=_textarea_one_line_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    def test_carets_not_jump_when_dragging_to_editable_content_boundary(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 3, 'Expect at least three words in the content.')

        # Goal: the selection is not changed after dragging the caret on the
        # Y-axis.
        target_content = words[1]

        self.long_press_on_word(el, 1)
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()

        # Drag the first caret up by 50px.
        self.actions.flick(el, caret1_x, caret1_y, caret1_x, caret1_y - 50).perform()
        self.assertEqual(target_content, sel.selected_content)

        # Drag the second caret down by 50px.
        self.actions.flick(el, caret2_x, caret2_y, caret2_x, caret2_y + 50).perform()
        self.assertEqual(target_content, sel.selected_content)
