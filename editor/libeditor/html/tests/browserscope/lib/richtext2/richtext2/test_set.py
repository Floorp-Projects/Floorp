#!/usr/bin/python2.5
#
# Copyright 2010 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the 'License')
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Rich Text 2 Test Definitions."""

__author__ = 'rolandsteiner@google.com (Roland Steiner)'

# Selection specifications used in test files:
#
# Caret/collapsed selections:
#
# SC: 'caret'    caret/collapsed selection
# SB: 'before'   caret/collapsed selection before element
# SA: 'after'    caret/collapsed selection after element
# SS: 'start'    caret/collapsed selection at the start of the element (before first child/at text pos. 0)
# SE: 'end'      caret/collapsed selection at the end of the element (after last child/at text pos. n)
# SX: 'betwixt'  collapsed selection between elements
#
# Range selections:
#
# SO: 'outside'  selection wraps element in question
# SI: 'inside'   selection is inside of element in question
# SW: 'wrap'     as SI, but also wraps all children of element
# SL: 'left'     oblique selection - starts outside element and ends inside
# SR: 'right'    oblique selection - starts inside element and ends outside
# SM: 'mixed'    selection starts and ends in different elements
#
# SxR: selection is reversed
#
# Sxn or SxRn    selection applies to element #n of several identical

import logging

from categories import test_set_base

# common to the RichText2 suite
from categories.richtext2 import common

# tests
from categories.richtext2.tests.apply         import APPLY_TESTS
from categories.richtext2.tests.applyCSS      import APPLY_TESTS_CSS
from categories.richtext2.tests.change        import CHANGE_TESTS
from categories.richtext2.tests.changeCSS     import CHANGE_TESTS_CSS
from categories.richtext2.tests.delete        import DELETE_TESTS
from categories.richtext2.tests.forwarddelete import FORWARDDELETE_TESTS
from categories.richtext2.tests.insert        import INSERT_TESTS
from categories.richtext2.tests.selection     import SELECTION_TESTS
from categories.richtext2.tests.unapply       import UNAPPLY_TESTS
from categories.richtext2.tests.unapplyCSS    import UNAPPLY_TESTS_CSS

from categories.richtext2.tests.querySupported import QUERYSUPPORTED_TESTS
from categories.richtext2.tests.queryEnabled   import QUERYENABLED_TESTS
from categories.richtext2.tests.queryIndeterm  import QUERYINDETERM_TESTS
from categories.richtext2.tests.queryState     import QUERYSTATE_TESTS, QUERYSTATE_TESTS_CSS
from categories.richtext2.tests.queryValue     import QUERYVALUE_TESTS, QUERYVALUE_TESTS_CSS


_SELECTION_TESTS_COUNT      = len([t['id'] for c in common.CLASSES for g in SELECTION_TESTS.get(c, []) for t in g['tests']])

_APPLY_TESTS_COUNT          = len([t['id'] for c in common.CLASSES for g in APPLY_TESTS.get(c, []) for t in g['tests']])
_APPLY_TESTS_CSS_COUNT      = len([t['id'] for c in common.CLASSES for g in APPLY_TESTS_CSS.get(c, []) for t in g['tests']])
_CHANGE_TESTS_COUNT         = len([t['id'] for c in common.CLASSES for g in CHANGE_TESTS.get(c, []) for t in g['tests']])
_CHANGE_TESTS_CSS_COUNT     = len([t['id'] for c in common.CLASSES for g in CHANGE_TESTS_CSS.get(c, []) for t in g['tests']])
_UNAPPLY_TESTS_COUNT        = len([t['id'] for c in common.CLASSES for g in UNAPPLY_TESTS.get(c, []) for t in g['tests']])
_UNAPPLY_TESTS_CSS_COUNT    = len([t['id'] for c in common.CLASSES for g in UNAPPLY_TESTS_CSS.get(c, []) for t in g['tests']])
_DELETE_TESTS_COUNT         = len([t['id'] for c in common.CLASSES for g in DELETE_TESTS.get(c, []) for t in g['tests']])
_FORWARDDELETE_TESTS_COUNT  = len([t['id'] for c in common.CLASSES for g in FORWARDDELETE_TESTS.get(c, []) for t in g['tests']])
_INSERT_TESTS_COUNT         = len([t['id'] for c in common.CLASSES for g in INSERT_TESTS.get(c, []) for t in g['tests']])

_SELECTION_RESULTS_COUNT    = _APPLY_TESTS_COUNT + \
                              _APPLY_TESTS_CSS_COUNT + \
                              _CHANGE_TESTS_COUNT + \
                              _CHANGE_TESTS_CSS_COUNT + \
                              _UNAPPLY_TESTS_COUNT + \
                              _UNAPPLY_TESTS_CSS_COUNT + \
                              _DELETE_TESTS_COUNT + \
                              _FORWARDDELETE_TESTS_COUNT + \
                              _INSERT_TESTS_COUNT       

_QUERYSUPPORTED_TESTS_COUNT = len([t['id'] for c in common.CLASSES for g in QUERYSUPPORTED_TESTS.get(c, []) for t in g['tests']])
_QUERYENABLED_TESTS_COUNT   = len([t['id'] for c in common.CLASSES for g in QUERYENABLED_TESTS.get(c, []) for t in g['tests']])
_QUERYINDETERM_TESTS_COUNT  = len([t['id'] for c in common.CLASSES for g in QUERYINDETERM_TESTS.get(c, []) for t in g['tests']])
_QUERYSTATE_TESTS_COUNT     = len([t['id'] for c in common.CLASSES for g in QUERYSTATE_TESTS.get(c, []) for t in g['tests']])
_QUERYSTATE_TESTS_CSS_COUNT = len([t['id'] for c in common.CLASSES for g in QUERYSTATE_TESTS_CSS.get(c, []) for t in g['tests']])
_QUERYVALUE_TESTS_COUNT     = len([t['id'] for c in common.CLASSES for g in QUERYVALUE_TESTS.get(c, []) for t in g['tests']])
_QUERYVALUE_TESTS_CSS_COUNT = len([t['id'] for c in common.CLASSES for g in QUERYVALUE_TESTS_CSS.get(c, []) for t in g['tests']])

TEST_CATEGORIES = {
  'selection':        { 'count':  _SELECTION_TESTS_COUNT,
                        'short':  'Selection',
                        'long':   '''These tests verify that selection commands are honored correctly.
                                  The expected and actual outputs are shown.'''},
  'apply':            { 'count':  _APPLY_TESTS_COUNT,
                        'short':  'Apply Format',
                        'long':   '''These tests use execCommand to apply formatting to plain text,
                                  with styleWithCSS being set to false.
                                  The expected and actual outputs are shown.'''},
  'applyCSS':         { 'count':  _APPLY_TESTS_CSS_COUNT,
                        'short':  'Apply Format, styleWithCSS',
                        'long':   '''These tests use execCommand to apply formatting to plain text,
                                  with styleWithCSS being set to true.
                                  The expected and actual outputs are shown.'''},
  'change':           { 'count':  _CHANGE_TESTS_COUNT,
                        'short':  'Change Format',
                        'long':   '''These tests are similar to the unapply tests, except that they're for
                                  execCommands which take an argument (fontname, fontsize, etc.). They apply
                                  the execCommand to text which already has some formatting, in order to change
                                  it. styleWithCSS is being set to false.
                                  The expected and actual outputs are shown.'''},
  'changeCSS':        { 'count':  _CHANGE_TESTS_CSS_COUNT,
                        'short':  'Change Format, styleWithCSS',
                        'long':   '''These tests are similar to the unapply tests, except that they're for
                                  execCommands which take an argument (fontname, fontsize, etc.). They apply
                                  the execCommand to text which already has some formatting, in order to change
                                  it. styleWithCSS is being set to true.
                                  The expected and actual outputs are shown.'''},
  'unapply':          { 'count':  _UNAPPLY_TESTS_COUNT,
                        'short':  'Unapply Format',
                        'long':   '''These tests put different combinations of HTML into a contenteditable
                                  iframe, and then run an execCommand to attempt to remove the formatting the
                                  HTML applies. For example, there are tests to check if
                                  bold styling from &lt;b&gt;, &lt;strong&gt;, and &lt;span
                                  style="font-weight:normal"&gt; are all removed by the bold execCommand.
                                  It is important that browsers can remove all variations of a style, not just
                                  the variation the browser applies on its own, because it's quite possible
                                  that a web application could allow editing with multiple browsers, or that
                                  users could paste content into the contenteditable region.
                                  For these tests, styleWithCSS is set to false.
                                  The expected and actual outputs are shown.'''},
  'unapplyCSS':       { 'count':  _UNAPPLY_TESTS_CSS_COUNT,
                        'short':  'Unapply Format, styleWithCSS',
                        'long':   '''These tests put different combinations of HTML into a contenteditable
                                  iframe, and then run an execCommand to attempt to remove the formatting the
                                  HTML applies. For example, there are tests to check if
                                  bold styling from &lt;b&gt;, &lt;strong&gt;, and &lt;span
                                  style="font-weight:normal"&gt; are all removed by the bold execCommand.
                                  It is important that browsers can remove all variations of a style, not just
                                  the variation the browser applies on its own, because it's quite possible
                                  that a web application could allow editing with multiple browsers, or that
                                  users could paste content into the contenteditable region.
                                  For these tests, styleWithCSS is set to true.
                                  The expected and actual outputs are shown.'''},
  'delete':           { 'count':  _DELETE_TESTS_COUNT,
                        'short':  'Delete Content',
                        'long':   '''These tests verify that 'delete' commands are executed correctly.
                                  Note that 'delete' commands are supposed to have the same result as if the
                                  user had hit the 'BackSpace' (NOT 'Delete'!) key.
                                  The expected and actual outputs are shown.'''},
  'forwarddelete':    { 'count':  _FORWARDDELETE_TESTS_COUNT,
                        'short':  'Forward-Delete Content',
                        'long':   '''These tests verify that 'forwarddelete' commands are executed correctly.
                                  Note that 'forwarddelete' commands are supposed to have the same result as if
                                  the user had hit the 'Delete' key.
                                  The expected and actual outputs are shown.'''},
  'insert':           { 'count':  _INSERT_TESTS_COUNT,
                        'short':  'Insert Content',
                        'long':   '''These tests verify that the various 'insert' and 'create' commands, that
                                  create a single HTML element, rather than wrapping existing content, are
                                  executed correctly. (Commands that wrap existing HTML are part of the 'apply'
                                  and 'applyCSS' categories.)
                                  The expected and actual outputs are shown.'''},
  'selectionResult':  { 'count':  _SELECTION_RESULTS_COUNT,
                        'short':  'Selection Results',
                        'long':   '''Number of cases within those tests that manipulate HTML
                                  (categories 'Apply', 'Change', 'Unapply', 'Delete', 'ForwardDelete', 'Insert')
                                  where the result selection matched the expectation.'''},
  'querySupported':   { 'count':  _QUERYSUPPORTED_TESTS_COUNT,
                        'short':  'q.C.Supported Function',
                        'long':   '''These tests verify that the 'queryCommandSupported()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to false.
                                  The expected and actual results are shown.'''},
  'queryEnabled':     { 'count':  _QUERYENABLED_TESTS_COUNT,
                        'short':  'q.C.Enabled Function',
                        'long':   '''These tests verify that the 'queryCommandEnabled()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to false.
                                  The expected and actual results are shown.'''},
  'queryIndeterm':    { 'count':  _QUERYINDETERM_TESTS_COUNT,
                        'short':  'q.C.Indeterm Function',
                        'long':   '''These tests verify that the 'queryCommandIndeterm()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to false.
                                  The expected and actual results are shown.'''},
  'queryState':       { 'count':  _QUERYSTATE_TESTS_COUNT,
                        'short':  'q.C.State Function',
                        'long':   '''These tests verify that the 'queryCommandState()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to false.
                                  The expected and actual results are shown.'''},
  'queryStateCSS':    { 'count':  _QUERYSTATE_TESTS_CSS_COUNT,
                        'short':  'q.C.State Function, styleWithCSS',
                        'long':   '''These tests verify that the 'queryCommandState()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to true.
                                  The expected and actual results are shown.'''},
  'queryValue':       { 'count':  _QUERYVALUE_TESTS_COUNT,
                        'short':  'q.C.Value Function',
                        'long':   '''These tests verify that the 'queryCommandValue()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to false.
                                  The expected and actual results are shown.'''},
  'queryValueCSS':    { 'count':  _QUERYVALUE_TESTS_CSS_COUNT,
                        'short':  'q.C.Value Function, styleWithCSS',
                        'long':   '''These tests verify that the 'queryCommandValue()' function return
                                  a correct result given a certain set-up. styleWithCSS is being set to true.
                                  The expected and actual results are shown.'''}
}

# Category tests:
# key, short description, documentation, # of tests

class RichText2TestCategory(test_set_base.TestBase):
  TESTS_URL_PATH = '/%s/test' % common.CATEGORY
  def __init__(self, key):
    test_set_base.TestBase.__init__(
        self,
        key = key,
        name = TEST_CATEGORIES[key]['short'],
        url = self.TESTS_URL_PATH,
        doc = TEST_CATEGORIES[key]['long'],
        min_value = 0,
        max_value = TEST_CATEGORIES[key]['count'],
        cell_align = 'center')

# Explicitly list categories rather than using a list comprehension, to preserve order
_CATEGORIES_SET = [
  RichText2TestCategory('selection'),
  RichText2TestCategory('apply'),
  RichText2TestCategory('applyCSS'),
  RichText2TestCategory('change'),
  RichText2TestCategory('changeCSS'),
  RichText2TestCategory('unapply'),
  RichText2TestCategory('unapplyCSS'),
  RichText2TestCategory('delete'),
  RichText2TestCategory('forwarddelete'),
  RichText2TestCategory('insert'),
  RichText2TestCategory('selectionResult'),
  RichText2TestCategory('querySupported'),
  RichText2TestCategory('queryEnabled'),
  RichText2TestCategory('queryIndeterm'),
  RichText2TestCategory('queryState'),
  RichText2TestCategory('queryStateCSS'),
  RichText2TestCategory('queryValue'),
  RichText2TestCategory('queryValueCSS'),
]


class RichText2TestSet(test_set_base.TestSet):

  def GetTestScoreAndDisplayValue(self, test_key, raw_scores):
    """Get a score and a text string to output to the display.

    Args:
      test_key: a key for a test_set sub-category.
      raw_scores: a dict of raw_scores indexed by test keys.
    Returns:
      score, display_value
          # score is from 0 to 100.
          # display_value is the text for the cell.
    """
    score = raw_scores.get(test_key)
    category = TEST_CATEGORIES[test_key]
    if score is None or category is None:
      return 0, ''
    count = category['count']
    percent = int(round(100.0 * score / count))
    display = '%s/%s' % (score, count)
    return percent, display 

  def GetRowScoreAndDisplayValue(self, results):
    """Get the overall score and text string for this row of results data.

    Args:
      results: {
          'test_key_1': {'score': score_1, 'raw_score': raw_score_1, ...},
          'test_key_2': {'score': score_2, 'raw_score': raw_score_2, ...},
          ...
          }
    Returns:
      score, display_value
          # score is from 0 to 100.
          # display_value is the text for the cell.
    """
    total_passed = 0
    total_tests = 0
    for test_key, test_results in results.items():
      display_test = test_results['display']
      if display_test == '':
        # If we ever see display_test == '', we know we can just walk away.
        return 0, ''
      passed, total = display_test.split('/')
      total_passed += int(passed)
      total_tests += int(total)
    display = '%s/%s' % (total_passed, total_tests)
    score = int(round(100.0 * total_passed / total_tests))
    return score, display

TEST_SET = RichText2TestSet(
    category = common.CATEGORY,
    category_name = 'Rich Text',
    summary_doc = 'New suite of tests to see how well editor controls work with a variety of HTML.',
    tests = _CATEGORIES_SET,
    test_page = "richtext2/run",
)

