# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette import MarionetteTestCase, SkipTest
from marionette_driver.selection import SelectionManager
from marionette_driver.gestures import long_press_without_contextmenu


class CommonCaretsTestCase2(object):
    '''Common test cases for a selection with a two carets.

    To run these test cases, a subclass must inherit from both this class and
    MarionetteTestCase.

    '''
    _long_press_time = 1        # 1 second

    def setUp(self):
        # Code to execute before a tests are run.
        MarionetteTestCase.setUp(self)
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


class SelectionCaretsTestCase2(CommonCaretsTestCase2, MarionetteTestCase):
    def setUp(self):
        super(SelectionCaretsTestCase2, self).setUp()
        self.carets_tested_pref = 'selectioncaret.enabled'
        self.carets_disabled_pref = 'layout.accessiblecaret.enabled'


class AccessibleCaretSelectionModeTestCase2(CommonCaretsTestCase2, MarionetteTestCase):
    def setUp(self):
        super(AccessibleCaretSelectionModeTestCase2, self).setUp()
        self.carets_tested_pref = 'layout.accessiblecaret.enabled'
        self.carets_disabled_pref = 'selectioncaret.enabled'
