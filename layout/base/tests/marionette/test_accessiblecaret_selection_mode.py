# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys
import os
import unittest

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from selection import (
    CaretActions,
    SelectionManager,
)
from marionette_driver.by import By
from marionette_driver.keys import Keys
from marionette_harness.marionette_test import (
    MarionetteTestCase,
    SkipTest,
    parameterized,
)


class AccessibleCaretSelectionModeTestCase(MarionetteTestCase):
    """Test cases for AccessibleCaret under selection mode."""

    # Element IDs.
    _input_id = "input"
    _input_padding_id = "input-padding"
    _input_size_id = "input-size"
    _textarea_id = "textarea"
    _textarea2_id = "textarea2"
    _textarea_disabled_id = "textarea-disabled"
    _textarea_one_line_id = "textarea-one-line"
    _textarea_rtl_id = "textarea-rtl"
    _contenteditable_id = "contenteditable"
    _contenteditable2_id = "contenteditable2"
    _content_id = "content"
    _content2_id = "content2"
    _non_selectable_id = "non-selectable"

    # Test html files.
    _selection_html = "layout/test_carets_selection.html"
    _multipleline_html = "layout/test_carets_multipleline.html"
    _multiplerange_html = "layout/test_carets_multiplerange.html"
    _longtext_html = "layout/test_carets_longtext.html"
    _iframe_html = "layout/test_carets_iframe.html"
    _iframe_scroll_html = "layout/test_carets_iframe_scroll.html"
    _display_none_html = "layout/test_carets_display_none.html"
    _svg_shapes_html = "layout/test_carets_svg_shapes.html"
    _key_scroll_html = "layout/test_carets_key_scroll.html"

    def setUp(self):
        # Code to execute before every test is running.
        super(AccessibleCaretSelectionModeTestCase, self).setUp()
        self.carets_tested_pref = "layout.accessiblecaret.enabled"
        self.prefs = {
            "layout.word_select.eat_space_to_next_word": False,
            self.carets_tested_pref: True,
            # To disable transition, or the caret may not be the desired
            # location yet, we cannot press a caret successfully.
            "layout.accessiblecaret.transition-duration": "0.0",
            # Enabled hapticfeedback on all platforms. The tests shouldn't crash
            # on platforms without hapticfeedback support.
            "layout.accessiblecaret.hapticfeedback": True,
        }
        self.marionette.set_prefs(self.prefs)
        self.actions = CaretActions(self.marionette)

    def tearDown(self):
        self.marionette.actions.release()
        super(AccessibleCaretSelectionModeTestCase, self).tearDown()

    def open_test_html(self, test_html):
        self.marionette.navigate(self.marionette.absolute_url(test_html))

    def word_offset(self, text, ordinal):
        "Get the character offset of the ordinal-th word in text."
        tokens = re.split(r"(\S+)", text)  # both words and spaces
        spaces = tokens[0::2]  # collect spaces at odd indices
        words = tokens[1::2]  # collect word at even indices

        if ordinal >= len(words):
            raise IndexError(
                "Only %d words in text, but got ordinal %d" % (len(words), ordinal)
            )

        # Cursor position of the targeting word is behind the the first
        # character in the word. For example, offset to 'def' in 'abc def' is
        # between 'd' and 'e'.
        offset = len(spaces[0]) + 1
        offset += sum(len(words[i]) + len(spaces[i + 1]) for i in range(ordinal))
        return offset

    def test_word_offset(self):
        text = " " * 3 + "abc" + " " * 3 + "def"

        self.assertTrue(self.word_offset(text, 0), 4)
        self.assertTrue(self.word_offset(text, 1), 10)
        with self.assertRaises(IndexError):
            self.word_offset(text, 2)

    def word_location(self, el, ordinal):
        """Get the location (x, y) of the ordinal-th word in el.

        The ordinal starts from 0.

        Note: this function has a side effect which changes focus to the
        target element el.

        """
        sel = SelectionManager(el)
        offset = self.word_offset(sel.content, ordinal)

        # Move the blinking cursor to the word.
        self.actions.click(element=el).perform()
        sel.move_cursor_to_front()
        sel.move_cursor_by_offset(offset)
        x, y = sel.cursor_location()

        return x, y

    def rect_relative_to_window(self, el):
        """Get element's bounding rectangle.

        This function is similar to el.rect, but the coordinate is relative to
        the top left corner of the window instead of the document.

        """
        return self.marionette.execute_script(
            """
            let rect = arguments[0].getBoundingClientRect();
            return {x: rect.x, y:rect.y, width: rect.width, height: rect.height};
            """,
            script_args=[el],
        )

    def long_press_on_location(self, el, x=None, y=None):
        """Long press the location (x, y) to select a word.

        If no (x, y) are given, it will be targeted at the center of the
        element. On Windows, those spaces after the word will also be selected.
        This function sends synthesized eMouseLongTap to gecko.

        """
        rect = self.rect_relative_to_window(el)
        target_x = rect["x"] + (x if x is not None else rect["width"] // 2)
        target_y = rect["y"] + (y if y is not None else rect["height"] // 2)

        self.marionette.execute_script(
            """
            let utils = window.windowUtils;
            utils.sendTouchEventToWindow('touchstart', [0],
                                         [arguments[0]], [arguments[1]],
                                         [1], [1], [0], [1], [0], [0], [0], 0);
            utils.sendMouseEventToWindow('mouselongtap', arguments[0], arguments[1],
                                          0, 1, 0);
            utils.sendTouchEventToWindow('touchend', [0],
                                         [arguments[0]], [arguments[1]],
                                         [1], [1], [0], [1], [0], [0], [0], 0);
            """,
            script_args=[target_x, target_y],
            sandbox="system",
        )

    def long_press_on_word(self, el, wordOrdinal):
        x, y = self.word_location(el, wordOrdinal)
        self.long_press_on_location(el, x, y)

    def to_unix_line_ending(self, s):
        """Changes all Windows/Mac line endings in s to UNIX line endings."""

        return s.replace("\r\n", "\n").replace("\r", "\n")

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_disabled_id, el_id=_textarea_disabled_id)
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
        self.assertTrue(len(words) >= 2, "Expect at least two words in the content.")
        target_content = words[0]

        # Goal: Select the first word.
        self.long_press_on_word(el, 0)

        # Ignore extra spaces selected after the word.
        self.assertEqual(target_content, sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    @parameterized(_textarea_id, el_id=_textarea_id)
    @parameterized(_textarea_disabled_id, el_id=_textarea_disabled_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_drag_carets(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, "Expect at least one word in the content.")

        # Goal: Select all text after the first word.
        target_content = original_content[len(words[0]) :]

        # Get the location of the carets at the end of the content for later
        # use.
        self.actions.click(element=el).perform()
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
    @parameterized(_textarea_disabled_id, el_id=_textarea_disabled_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_drag_swappable_carets(self, el_id):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, "Expect at least one word in the content.")

        target_content1 = words[0]
        target_content2 = original_content[len(words[0]) :]

        # Get the location of the carets at the end of the content for later
        # use.
        self.actions.click(element=el).perform()
        sel.select_all()
        end_caret_x, end_caret_y = sel.second_caret_location()

        self.long_press_on_word(el, 0)

        # Drag the first caret to the end and back to where it was
        # immediately. The selection range should not be collapsed.
        caret1_x, caret1_y = sel.first_caret_location()
        self.actions.flick(el, caret1_x, caret1_y, end_caret_x, end_caret_y).flick(
            el, end_caret_x, end_caret_y, caret1_x, caret1_y
        ).perform()
        self.assertEqual(target_content1, sel.selected_content)

        # Drag the first caret to the end.
        caret1_x, caret1_y = sel.first_caret_location()
        self.actions.flick(el, caret1_x, caret1_y, end_caret_x, end_caret_y).perform()
        self.assertEqual(target_content2, sel.selected_content)

    @parameterized(_input_id, el_id=_input_id)
    # Bug 1855083: Skip textarea tests due to high frequent intermittent.
    # @parameterized(_textarea_id, el_id=_textarea_id)
    # @parameterized(_textarea_disabled_id, el_id=_textarea_disabled_id)
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

    def _test_minimum_select_one_character(self, el):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, "Expect at least one word in the content.")

        # Get the location of the carets at the end of the content for later
        # use.
        sel.select_all()
        end_caret_x, end_caret_y = sel.second_caret_location()
        self.actions.click(element=el).perform()

        # Goal: Select the first character.
        target_content = original_content[0]

        self.long_press_on_word(el, 0)

        # Drag the second caret to the end of the content.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, end_caret_x, end_caret_y).perform()

        # Drag the second caret to the position of the first caret.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(el, caret2_x, caret2_y, caret1_x, caret1_y).perform()

        self.assertEqual(target_content, sel.selected_content)

    @parameterized(
        _input_id + "_to_" + _textarea_id, el1_id=_input_id, el2_id=_textarea_id
    )
    @parameterized(
        _input_id + "_to_" + _contenteditable_id,
        el1_id=_input_id,
        el2_id=_contenteditable_id,
    )
    @parameterized(
        _input_id + "_to_" + _content_id, el1_id=_input_id, el2_id=_content_id
    )
    @parameterized(
        _textarea_id + "_to_" + _input_id, el1_id=_textarea_id, el2_id=_input_id
    )
    @parameterized(
        _textarea_id + "_to_" + _contenteditable_id,
        el1_id=_textarea_id,
        el2_id=_contenteditable_id,
    )
    @parameterized(
        _textarea_id + "_to_" + _content_id, el1_id=_textarea_id, el2_id=_content_id
    )
    @parameterized(
        _contenteditable_id + "_to_" + _input_id,
        el1_id=_contenteditable_id,
        el2_id=_input_id,
    )
    @parameterized(
        _contenteditable_id + "_to_" + _textarea_id,
        el1_id=_contenteditable_id,
        el2_id=_textarea_id,
    )
    @parameterized(
        _contenteditable_id + "_to_" + _content_id,
        el1_id=_contenteditable_id,
        el2_id=_content_id,
    )
    @parameterized(
        _content_id + "_to_" + _input_id, el1_id=_content_id, el2_id=_input_id
    )
    @parameterized(
        _content_id + "_to_" + _textarea_id, el1_id=_content_id, el2_id=_textarea_id
    )
    @parameterized(
        _content_id + "_to_" + _contenteditable_id,
        el1_id=_content_id,
        el2_id=_contenteditable_id,
    )
    def test_long_press_changes_focus_from(self, el1_id, el2_id):
        self.open_test_html(self._selection_html)
        el1 = self.marionette.find_element(By.ID, el1_id)
        el2 = self.marionette.find_element(By.ID, el2_id)

        # Compute the content of the first word in el2.
        sel = SelectionManager(el2)
        original_content = sel.content
        words = original_content.split()
        target_content = words[0]

        # Goal: Click to focus el1, and then select the first word on el2.

        # We want to collect the location of the first word in el2 here
        # since self.word_location() has the side effect which would
        # change the focus.
        x, y = self.word_location(el2, 0)

        self.actions.click(element=el1).perform()
        self.long_press_on_location(el2, x, y)
        self.assertEqual(target_content, sel.selected_content)

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
    @parameterized(_textarea_disabled_id, el_id=_textarea_disabled_id)
    @parameterized(_textarea_rtl_id, el_id=_textarea_rtl_id)
    @parameterized(_contenteditable_id, el_id=_contenteditable_id)
    @parameterized(_content_id, el_id=_content_id)
    def test_handle_tilt_when_carets_overlap_each_other(self, el_id):
        """Test tilt handling when carets overlap to each other.

        Let the two carets overlap each other. If they are set to tilted
        successfully, click on the tilted carets should not cause the selection
        to be collapsed and the carets should be draggable.

        """
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, el_id)
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, "Expect at least one word in the content.")

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
        caret_width = float(self.marionette.get_pref("layout.accessiblecaret.width"))
        caret_margin_left = float(
            self.marionette.get_pref("layout.accessiblecaret.margin-left")
        )
        tilt_right_margin_left = 0.41 * caret_width
        tilt_left_margin_left = -0.39 * caret_width

        left_caret_left_edge_x = caret3_x + caret_margin_left + tilt_left_margin_left
        self.actions.move(el, left_caret_left_edge_x + 2, caret3_y).click().perform()

        right_caret_right_edge_x = (
            caret4_x + caret_margin_left + tilt_right_margin_left + caret_width
        )
        self.actions.move(el, right_caret_right_edge_x - 2, caret4_y).click().perform()

        # Drag the first caret back to the initial selection, the first word.
        self.actions.flick(el, caret3_x, caret3_y, caret1_x, caret1_y).perform()

        self.assertEqual(target_content, sel.selected_content)

    def test_drag_caret_over_non_selectable_field(self):
        """Test dragging the caret over a non-selectable field.

        The selected content should exclude non-selectable elements and the
        second caret should appear in last range's position.

        """
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, "bd")
        sel3 = self.marionette.find_element(By.ID, "sel3")
        sel4 = self.marionette.find_element(By.ID, "sel4")
        sel6 = self.marionette.find_element(By.ID, "sel6")

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
        self.actions.flick(
            body, caret2_x, caret2_y, end_caret_x, end_caret_y, 1
        ).perform()
        self.assertEqual(
            self.to_unix_line_ending(sel.selected_content.strip()),
            "this 3\nuser can select this",
        )

        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(
            body, caret2_x, caret2_y, end_caret2_x, end_caret2_y, 1
        ).perform()
        self.assertEqual(
            self.to_unix_line_ending(sel.selected_content.strip()),
            "this 3\nuser can select this 4\nuser can select this 5\nuser",
        )

        # Drag first caret to target location
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(
            body, caret1_x, caret1_y, end_caret_x, end_caret_y, 1
        ).perform()
        self.assertEqual(
            self.to_unix_line_ending(sel.selected_content.strip()),
            "4\nuser can select this 5\nuser",
        )

    def test_drag_swappable_caret_over_non_selectable_field(self):
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, "bd")
        sel3 = self.marionette.find_element(By.ID, "sel3")
        sel4 = self.marionette.find_element(By.ID, "sel4")
        sel = SelectionManager(body)

        self.long_press_on_word(sel4, 3)
        (
            (end_caret1_x, end_caret1_y),
            (end_caret2_x, end_caret2_y),
        ) = sel.carets_location()

        self.long_press_on_word(sel3, 3)
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()

        # Drag the first caret down, which will across the second caret.
        self.actions.flick(
            body, caret1_x, caret1_y, end_caret1_x, end_caret1_y
        ).perform()
        self.assertEqual(
            self.to_unix_line_ending(sel.selected_content.strip()), "3\nuser can select"
        )

        # The old second caret becomes the first caret. Drag it down again.
        self.actions.flick(
            body, caret2_x, caret2_y, end_caret2_x, end_caret2_y
        ).perform()
        self.assertEqual(self.to_unix_line_ending(sel.selected_content.strip()), "this")

    def test_drag_caret_to_beginning_of_a_line(self):
        """Bug 1094056
        Test caret visibility when caret is dragged to beginning of a line
        """
        self.open_test_html(self._multiplerange_html)
        body = self.marionette.find_element(By.ID, "bd")
        sel1 = self.marionette.find_element(By.ID, "sel1")
        sel2 = self.marionette.find_element(By.ID, "sel2")

        # Select the first word in the second line
        self.long_press_on_word(sel2, 0)
        sel = SelectionManager(body)
        (
            (start_caret_x, start_caret_y),
            (end_caret_x, end_caret_y),
        ) = sel.carets_location()

        # Select target word in the first line
        self.long_press_on_word(sel1, 2)

        # Drag end caret to the beginning of the second line
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()
        self.actions.flick(
            body, caret2_x, caret2_y, start_caret_x, start_caret_y
        ).perform()

        # Drag end caret back to the target word
        self.actions.flick(
            body, start_caret_x, start_caret_y, caret2_x, caret2_y
        ).perform()

        self.assertEqual(self.to_unix_line_ending(sel.selected_content), "select")

    def test_select_word_inside_an_iframe(self):
        """Bug 1088552
        The scroll offset in iframe should be taken into consideration properly.
        In this test, we scroll content in the iframe to the bottom to cause a
        huge offset. If we use the right coordinate system, selection should
        work. Otherwise, it would be hard to trigger select word.
        """
        self.open_test_html(self._iframe_html)
        iframe = self.marionette.find_element(By.ID, "frame")

        # switch to inner iframe and scroll to the bottom
        self.marionette.switch_to_frame(iframe)
        self.marionette.execute_script('document.getElementById("bd").scrollTop += 999')

        # long press to select bottom text
        body = self.marionette.find_element(By.ID, "bd")
        sel = SelectionManager(body)
        self._bottomtext = self.marionette.find_element(By.ID, "bottomtext")
        self.long_press_on_location(self._bottomtext)

        self.assertNotEqual(self.to_unix_line_ending(sel.selected_content), "")

    def test_select_word_inside_an_unfocused_iframe(self):
        """Bug 1306634: Test we can long press to select a word in an unfocused iframe."""
        self.open_test_html(self._iframe_html)

        el = self.marionette.find_element(By.ID, self._input_id)
        sel = SelectionManager(el)

        # First, we select the first word in the input of the parent document.
        el_first_word = sel.content.split()[0]  # first world is "ABC"
        self.long_press_on_word(el, 0)
        self.assertEqual(el_first_word, sel.selected_content)

        # Then, we long press on the center of the iframe. It should select a
        # word inside of the document, not the placehoder in the parent
        # document.
        iframe = self.marionette.find_element(By.ID, "frame")
        self.long_press_on_location(iframe)
        self.marionette.switch_to_frame(iframe)
        body = self.marionette.find_element(By.ID, "bd")
        sel = SelectionManager(body)
        self.assertNotEqual("", sel.selected_content)

    def test_carets_initialized_in_display_none(self):
        """Test AccessibleCaretEventHub is properly initialized on a <html> with
        display: none.

        """
        self.open_test_html(self._display_none_html)
        html = self.marionette.find_element(By.ID, "html")
        content = self.marionette.find_element(By.ID, "content")

        # Remove 'display: none' from <html>
        self.marionette.execute_script(
            'arguments[0].style.display = "unset";', script_args=[html]
        )

        # If AccessibleCaretEventHub is initialized successfully, select a word
        # should work.
        self._test_long_press_to_select_a_word(content)

    @unittest.skip("Bug 1855083: High frequent intermittent.")
    def test_long_press_to_select_when_partial_visible_word_is_selected(self):
        self.open_test_html(self._selection_html)
        el = self.marionette.find_element(By.ID, self._input_size_id)
        sel = SelectionManager(el)

        original_content = sel.content
        words = original_content.split()

        # We cannot use self.long_press_on_word() for the second long press
        # on the first word because it has side effect that changes the
        # cursor position. We need to save the location of the first word to
        # be used later.
        word0_x, word0_y = self.word_location(el, 0)

        # Long press on the second word.
        self.long_press_on_word(el, 1)
        self.assertEqual(words[1], sel.selected_content)

        # Long press on the first word.
        self.long_press_on_location(el, word0_x, word0_y)
        self.assertEqual(words[0], sel.selected_content)

        # If the second caret is visible, it can be dragged to the position
        # of the first caret. After that, selection will contain only the
        # first character.
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
        self.assertTrue(len(words) >= 3, "Expect at least three words in the content.")

        # Goal: the selection is not changed after dragging the caret on the
        # Y-axis.
        target_content = words[1]

        self.long_press_on_word(el, 1)
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.carets_location()

        # Drag the first caret up by 40px.
        self.actions.flick(el, caret1_x, caret1_y, caret1_x, caret1_y - 40).perform()
        self.assertEqual(target_content, sel.selected_content)

        # Drag the second caret down by 50px.
        self.actions.flick(el, caret2_x, caret2_y, caret2_x, caret2_y + 50).perform()
        self.assertEqual(target_content, sel.selected_content)

    def test_carets_should_not_appear_when_long_pressing_svg_shapes(self):
        self.open_test_html(self._svg_shapes_html)

        rect = self.marionette.find_element(By.ID, "rect")
        text = self.marionette.find_element(By.ID, "text")

        sel = SelectionManager(text)
        num_words_in_text = len(sel.content.split())

        # Goal: the carets should not appear when long-pressing on the
        # unselectable SVG rect.

        # Get the position of the end of last word in text, i.e. the
        # position of the second caret when selecting the last word.
        self.long_press_on_word(text, num_words_in_text - 1)
        (_, _), (x2, y2) = sel.carets_location()

        # Long press to select the unselectable SVG rect.
        self.long_press_on_location(rect)
        (_, _), (x, y) = sel.carets_location()

        # Drag the second caret to (x2, y2).
        self.actions.flick(text, x, y, x2, y2).perform()

        # If the carets should appear on the rect, the selection will be
        # extended to cover all the words in text. Assert this should not
        # happen.
        self.assertNotEqual(sel.content, sel.selected_content.strip())

    def test_select_word_scroll_then_drag_caret(self):
        """Bug 1657256: Test select word, scroll page up , and then drag the second
        caret down to cover "EEEEEE".

        Note the selection should be extended to just cover "EEEEEE", not extend
        to other lines below "EEEEEE".

        """

        self.open_test_html(self._iframe_scroll_html)
        iframe = self.marionette.find_element(By.ID, "iframe")

        # Switch to the inner iframe.
        self.marionette.switch_to_frame(iframe)
        body = self.marionette.find_element(By.ID, "bd")
        sel = SelectionManager(body)

        # Select "EEEEEE" to get the y position of the second caret. This is the
        # y position we are going to drag the caret to.
        content2 = self.marionette.find_element(By.ID, self._content2_id)
        self.long_press_on_word(content2, 0)
        (_, _), (x, y2) = sel.carets_location()

        # Step 1: Select "DDDDDD".
        content = self.marionette.find_element(By.ID, self._content_id)
        self.long_press_on_word(content, 0)
        (_, _), (_, y1) = sel.carets_location()

        # The y-axis offset of the second caret needed to extend the selection.
        y_offset = y2 - y1

        # Step 2: Scroll the page upwards by 40px.
        scroll_offset = 40
        self.marionette.execute_script(
            "document.documentElement.scrollTop += arguments[0]",
            script_args=[scroll_offset],
        )

        # Step 3: Drag the second caret down.
        self.actions.flick(
            body, x, y1 - scroll_offset, x, y1 - scroll_offset + y_offset
        ).perform()

        self.assertEqual("DDDDDD EEEEEE", sel.selected_content)

    @unittest.skip("Bug 1855083: High frequent intermittent.")
    def test_carets_not_show_after_key_scroll_down_and_up(self):
        self.open_test_html(self._key_scroll_html)
        html = self.marionette.find_element(By.ID, "html")
        sel = SelectionManager(html)

        # Select "BBBBB" to get the position of the second caret. This is the
        # position to which we are going to drag the caret in the step 3.
        content2 = self.marionette.find_element(By.ID, self._content2_id)
        self.long_press_on_word(content2, 0)
        (_, _), (x2, y2) = sel.carets_location()

        # Step 1: Select "AAAAA".
        content = self.marionette.find_element(By.ID, self._content_id)
        self.long_press_on_word(content, 0)
        (_, _), (x1, y1) = sel.carets_location()

        # Step 2: Scroll the page down and up.
        #
        # FIXME: The selection highlight on "AAAAA" often disappears after page
        # up, which is the root cause of the intermittent. Longer pause between
        # the page down and page up might help, but it's not a great solution.
        self.actions.key_chain.send_keys(Keys.PAGE_DOWN).pause(1000).send_keys(
            Keys.PAGE_UP
        ).perform()
        self.assertEqual("AAAAA", sel.selected_content)

        # Step 3: The carets shouldn't show up after scrolling the page. We're
        # attempting to drag the second caret down so that if the bug occurs, we
        # can drag the second caret to extend the selection to "BBBBB".
        self.actions.flick(html, x1, y1, x2, y2).perform()
        self.assertNotEqual("AAAAA\nBBBBB", sel.selected_content)
