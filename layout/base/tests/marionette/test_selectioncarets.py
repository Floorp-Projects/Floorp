# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette import MarionetteTestCase
from marionette_driver.selection import SelectionManager
from marionette_driver.gestures import long_press_without_contextmenu


class CommonCaretsTestCase(object):
    '''Common test cases for a selection with a two carets.

    To run these test cases, a subclass must inherit from both this class and
    MarionetteTestCase.

    '''
    _long_press_time = 1        # 1 second
    _input_selector = (By.ID, 'input')
    _textarea_selector = (By.ID, 'textarea')
    _textarea_rtl_selector = (By.ID, 'textarea_rtl')
    _contenteditable_selector = (By.ID, 'contenteditable')
    _content_selector = (By.ID, 'content')
    _textarea2_selector = (By.ID, 'textarea2')
    _contenteditable2_selector = (By.ID, 'contenteditable2')
    _content2_selector = (By.ID, 'content2')

    def setUp(self):
        # Code to execute before a tests are run.
        super(CommonCaretsTestCase, self).setUp()
        self.actions = Actions(self.marionette)

        # The carets to be tested.
        self.carets_tested_pref = None

        # The carets to be disabled in this test suite.
        self.carets_disabled_pref = None

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

    def open_test_html(self, enabled=True):
        '''Open html for testing and locate elements, and enable/disable touch
        caret.'''
        self.set_pref(self.carets_tested_pref, enabled)
        self.set_pref(self.carets_disabled_pref, False)

        test_html = self.marionette.absolute_url('test_selectioncarets.html')
        self.marionette.navigate(test_html)

        self._input = self.marionette.find_element(*self._input_selector)
        self._textarea = self.marionette.find_element(*self._textarea_selector)
        self._textarea_rtl = self.marionette.find_element(*self._textarea_rtl_selector)
        self._contenteditable = self.marionette.find_element(*self._contenteditable_selector)
        self._content = self.marionette.find_element(*self._content_selector)

    def open_test_html2(self, enabled=True):
        '''Open html for testing and locate elements, and enable/disable touch
        caret.'''
        self.set_pref(self.carets_tested_pref, enabled)
        self.set_pref(self.carets_disabled_pref, False)

        test_html2 = self.marionette.absolute_url('test_selectioncarets_multipleline.html')
        self.marionette.navigate(test_html2)

        self._textarea2 = self.marionette.find_element(*self._textarea2_selector)
        self._contenteditable2 = self.marionette.find_element(*self._contenteditable2_selector)
        self._content2 = self.marionette.find_element(*self._content2_selector)

    def _first_word_location(self, el):
        '''Get the location (x, y) of the first word in el.

        Note: this function has a side effect which changes focus to the
        target element el.

        '''
        sel = SelectionManager(el)

        # Move caret behind the first character to get the location of the first
        # word.
        el.tap()
        sel.move_caret_to_front()
        sel.move_caret_by_offset(1)

        return sel.caret_location()

    def _long_press_to_select(self, el, x, y):
        '''Long press the location (x, y) to select a word.

        SelectionCarets should appear. On Windows, those spaces after the
        word will also be selected.

        '''
        long_press_without_contextmenu(self.marionette, el, self._long_press_time,
                                       x, y)

    def _test_long_press_to_select_a_word(self, el, assertFunc):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 2, 'Expect at least two words in the content.')
        target_content = words[0]

        # Goal: Select the first word.
        x, y = self._first_word_location(el)
        self._long_press_to_select(el, x, y)

        # Ignore extra spaces selected after the word.
        assertFunc(target_content, sel.selected_content.rstrip())

    def _test_move_selection_carets(self, el, assertFunc):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Goal: Select all text after the first word.
        target_content = original_content[len(words[0]):]

        # Get the location of the selection carets at the end of the content for
        # later use.
        el.tap()
        sel.select_all()
        (_, _), (end_caret_x, end_caret_y) = sel.selection_carets_location()

        x, y = self._first_word_location(el)
        self._long_press_to_select(el, x, y)

        # Move the right caret to the end of the content.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(el, caret2_x, caret2_y, end_caret_x, end_caret_y).perform()

        # Move the left caret to the previous position of the right caret.
        self.actions.flick(el, caret1_x, caret1_y, caret2_x, caret2_y).perform()

        # Ignore extra spaces at the beginning of the content in comparison.
        assertFunc(target_content.lstrip(), sel.selected_content.lstrip())

    def _test_minimum_select_one_character(self, el, assertFunc,
                                           x=None, y=None):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Get the location of the selection carets at the end of the content for
        # later use.
        sel.select_all()
        (_, _), (end_caret_x, end_caret_y) = sel.selection_carets_location()
        el.tap()

        # Goal: Select the first character.
        target_content = original_content[0]

        if x and y:
            # If we got x and y from the arguments, use it as a hint of the
            # location of the first word
            pass
        else:
            x, y = self._first_word_location(el)
        self._long_press_to_select(el, x, y)

        # Move the right caret to the end of the content.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(el, caret2_x, caret2_y, end_caret_x, end_caret_y).perform()

        # Move the right caret to the position of the left caret.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(el, caret2_x, caret2_y, caret1_x, caret1_y).perform()

        assertFunc(target_content, sel.selected_content)

    def _test_focus_obtained_by_long_press(self, el1, el2):
        '''Test the focus could be changed from el1 to el2 by long press.

        If the focus is changed to e2 successfully, SelectionCarets should
        appear and could be dragged.

        '''
        # Goal: Tap to focus el1, and then select the first character on
        # el2.

        # We want to collect the location of the first word in el2 here
        # since self._first_word_location() has the side effect which would
        # change the focus.
        x, y = self._first_word_location(el2)
        el1.tap()
        self._test_minimum_select_one_character(el2, self.assertEqual,
                                                x=x, y=y)

    def _test_handle_tilt_when_carets_overlap_to_each_other(self, el, assertFunc):
        '''Test tilt handling when carets overlap to each other.

        Let SelectionCarets overlap to each other. If SelectionCarets are set
        to tilted successfully, tapping the tilted carets should not cause the
        selection to be collapsed and the carets should be draggable.
        '''

        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(len(words) >= 1, 'Expect at least one word in the content.')

        # Goal: Select the first word.
        x, y = self._first_word_location(el)
        self._long_press_to_select(el, x, y)
        target_content = sel.selected_content

        # Move the left caret to the position of the right caret to trigger
        # carets overlapping.
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(el, caret1_x, caret1_y, caret2_x, caret2_y).perform()

        # We make two hit tests targeting the left edge of the left tilted caret
        # and the right edge of the right tilted caret. If either of the hits is
        # missed, selection would be collapsed and both carets should not be
        # draggable.
        (caret3_x, caret3_y), (caret4_x, caret4_y) = sel.selection_carets_location()

        # The following values are from ua.css.
        caret_width = 44
        caret_margin_left = -23
        tilt_right_margin_left = 18
        tilt_left_margin_left = -17

        left_caret_left_edge_x = caret3_x + caret_margin_left + tilt_left_margin_left
        el.tap(left_caret_left_edge_x + 2, caret3_y)

        right_caret_right_edge_x = (caret4_x + caret_margin_left +
                                    tilt_right_margin_left + caret_width)
        el.tap(right_caret_right_edge_x - 2, caret4_y)

        # Drag the left caret back to the initial selection, the first word.
        self.actions.flick(el, caret3_x, caret3_y, caret1_x, caret1_y).perform()

        assertFunc(target_content, sel.selected_content)

    ########################################################################
    # <input> test cases with selection carets enabled
    ########################################################################
    def test_input_long_press_to_select_a_word(self):
        self.open_test_html()
        self._test_long_press_to_select_a_word(self._input, self.assertEqual)

    def test_input_move_selection_carets(self):
        self.open_test_html()
        self._test_move_selection_carets(self._input, self.assertEqual)

    def test_input_minimum_select_one_character(self):
        self.open_test_html()
        self._test_minimum_select_one_character(self._input, self.assertEqual)

    def test_input_focus_obtained_by_long_press_from_textarea(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._textarea, self._input)

    def test_input_focus_obtained_by_long_press_from_contenteditable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._contenteditable, self._input)

    def test_input_focus_obtained_by_long_press_from_content_non_editable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._content, self._input)

    def test_input_handle_tilt_when_carets_overlap_to_each_other(self):
        self.open_test_html()
        self._test_handle_tilt_when_carets_overlap_to_each_other(self._input, self.assertEqual)

    ########################################################################
    # <input> test cases with selection carets disabled
    ########################################################################
    def test_input_long_press_to_select_a_word_disabled(self):
        self.open_test_html(enabled=False)
        self._test_long_press_to_select_a_word(self._input, self.assertNotEqual)

    def test_input_move_selection_carets_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_selection_carets(self._input, self.assertNotEqual)

    ########################################################################
    # <textarea> test cases with selection carets enabled
    ########################################################################
    def test_textarea_long_press_to_select_a_word(self):
        self.open_test_html()
        self._test_long_press_to_select_a_word(self._textarea, self.assertEqual)

    def test_textarea_move_selection_carets(self):
        self.open_test_html()
        self._test_move_selection_carets(self._textarea, self.assertEqual)

    def test_textarea_minimum_select_one_character(self):
        self.open_test_html()
        self._test_minimum_select_one_character(self._textarea, self.assertEqual)

    def test_textarea_focus_obtained_by_long_press_from_input(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._input, self._textarea)

    def test_textarea_focus_obtained_by_long_press_from_contenteditable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._contenteditable, self._textarea)

    def test_textarea_focus_obtained_by_long_press_from_content_non_editable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._content, self._textarea)

    def test_textarea_handle_tilt_when_carets_overlap_to_each_other(self):
        self.open_test_html()
        self._test_handle_tilt_when_carets_overlap_to_each_other(self._textarea, self.assertEqual)

    ########################################################################
    # <textarea> test cases with selection carets disabled
    ########################################################################
    def test_textarea_long_press_to_select_a_word_disabled(self):
        self.open_test_html(enabled=False)
        self._test_long_press_to_select_a_word(self._textarea, self.assertNotEqual)

    def test_textarea_move_selection_carets_disable(self):
        self.open_test_html(enabled=False)
        self._test_move_selection_carets(self._textarea, self.assertNotEqual)

    ########################################################################
    # <textarea> right-to-left test cases with selection carets enabled
    ########################################################################
    def test_textarea_rtl_long_press_to_select_a_word(self):
        self.open_test_html()
        self._test_long_press_to_select_a_word(self._textarea_rtl, self.assertEqual)

    def test_textarea_rtl_move_selection_carets(self):
        self.open_test_html()
        self._test_move_selection_carets(self._textarea_rtl, self.assertEqual)

    def test_textarea_rtl_minimum_select_one_character(self):
        self.open_test_html()
        self._test_minimum_select_one_character(self._textarea_rtl, self.assertEqual)

    ########################################################################
    # <textarea> right-to-left test cases with selection carets disabled
    ########################################################################
    def test_textarea_rtl_long_press_to_select_a_word_disabled(self):
        self.open_test_html(enabled=False)
        self._test_long_press_to_select_a_word(self._textarea_rtl, self.assertNotEqual)

    def test_textarea_rtl_move_selection_carets_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_selection_carets(self._textarea_rtl, self.assertNotEqual)

    ########################################################################
    # <div> contenteditable test cases with selection carets enabled
    ########################################################################
    def test_contenteditable_long_press_to_select_a_word(self):
        self.open_test_html()
        self._test_long_press_to_select_a_word(self._contenteditable, self.assertEqual)

    def test_contenteditable_move_selection_carets(self):
        self.open_test_html()
        self._test_move_selection_carets(self._contenteditable, self.assertEqual)

    def test_contenteditable_minimum_select_one_character(self):
        self.open_test_html()
        self._test_minimum_select_one_character(self._contenteditable, self.assertEqual)

    def test_contenteditable_focus_obtained_by_long_press_from_input(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._input, self._contenteditable)

    def test_contenteditable_focus_obtained_by_long_press_from_textarea(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._textarea, self._contenteditable)

    def test_contenteditable_focus_obtained_by_long_press_from_content_non_editable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._content, self._contenteditable)

    def test_contenteditable_handle_tilt_when_carets_overlap_to_each_other(self):
        self.open_test_html()
        self._test_handle_tilt_when_carets_overlap_to_each_other(self._contenteditable, self.assertEqual)

    ########################################################################
    # <div> contenteditable test cases with selection carets disabled
    ########################################################################
    def test_contenteditable_long_press_to_select_a_word_disabled(self):
        self.open_test_html(enabled=False)
        self._test_long_press_to_select_a_word(self._contenteditable, self.assertNotEqual)

    def test_contenteditable_move_selection_carets_disabled(self):
        self.open_test_html(enabled=False)
        self._test_move_selection_carets(self._contenteditable, self.assertNotEqual)

    ########################################################################
    # <div> non-editable test cases with selection carets enabled
    ########################################################################
    def test_content_non_editable_long_press_to_select_a_word(self):
        self.open_test_html()
        self._test_long_press_to_select_a_word(self._content, self.assertEqual)

    def test_content_non_editable_move_selection_carets(self):
        self.open_test_html()
        self._test_move_selection_carets(self._content, self.assertEqual)

    def test_content_non_editable_minimum_select_one_character_by_selection(self):
        self.open_test_html()
        self._test_minimum_select_one_character(self._content, self.assertEqual)

    def test_content_non_editable_focus_obtained_by_long_press_from_input(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._input, self._content)

    def test_content_non_editable_focus_obtained_by_long_press_from_textarea(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._textarea, self._content)

    def test_content_non_editable_focus_obtained_by_long_press_from_contenteditable(self):
        self.open_test_html()
        self._test_focus_obtained_by_long_press(self._contenteditable, self._content)

    def test_content_non_editable_handle_tilt_when_carets_overlap_to_each_other(self):
        self.open_test_html()
        self._test_handle_tilt_when_carets_overlap_to_each_other(self._content, self.assertEqual)

    ########################################################################
    # <textarea> (multi-lines) test cases with selection carets enabled
    ########################################################################
    def test_textarea2_minimum_select_one_character(self):
        self.open_test_html2()
        self._test_minimum_select_one_character(self._textarea2, self.assertEqual)

    ########################################################################
    # <div> contenteditable2 (multi-lines) test cases with selection carets enabled
    ########################################################################
    def test_contenteditable2_minimum_select_one_character(self):
        self.open_test_html2()
        self._test_minimum_select_one_character(self._contenteditable2, self.assertEqual)

    ########################################################################
    # <div> non-editable2 (multi-lines) test cases with selection carets enabled
    ########################################################################
    def test_content_non_editable2_minimum_select_one_character(self):
        self.open_test_html2()
        self._test_minimum_select_one_character(self._content2, self.assertEqual)


class SelectionCaretsTestCase(CommonCaretsTestCase, MarionetteTestCase):
    def setUp(self):
        super(SelectionCaretsTestCase, self).setUp()
        self.carets_tested_pref = 'selectioncaret.enabled'
        self.carets_disabled_pref = 'layout.accessiblecaret.enabled'


class AccessibleCaretSelectionModeTestCase(CommonCaretsTestCase, MarionetteTestCase):
    def setUp(self):
        super(AccessibleCaretSelectionModeTestCase, self).setUp()
        self.carets_tested_pref = 'layout.accessiblecaret.enabled'
        self.carets_disabled_pref = 'selectioncaret.enabled'
