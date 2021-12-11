/**
 * @fileoverview 
 * Common constants and variables used in the RTE test suite.
 *
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the 'License')
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @version 0.1
 * @author rolandsteiner@google.com
 */

// Constant for indicating a test setup is unsupported or incorrect
// (threw exception).
var INTERNAL_ERR           = 'INTERNAL ERROR: ';
var SETUP_EXCEPTION        = 'SETUP EXCEPTION: ';
var EXECUTION_EXCEPTION    = 'EXECUTION EXCEPTION: ';
var VERIFICATION_EXCEPTION = 'VERIFICATION EXCEPTION: ';

var SETUP_CONTAINER          = 'WHEN INITIALIZING TEST CONTAINER';
var SETUP_BAD_SELECTION_SPEC = 'BAD SELECTION SPECIFICATION IN TEST OR EXPECTATION STRING';
var SETUP_HTML               = 'WHEN SETTING TEST HTML';
var SETUP_SELECTION          = 'WHEN SETTING SELECTION';
var SETUP_NOCOMMAND          = 'NO COMMAND, GENERAL FUNCTION OR QUERY FUNCTION GIVEN';
var HTML_COMPARISON          = 'WHEN COMPARING OUTPUT HTML';

// Exceptiona to be thrown on unsupported selection operations
var SELMODIFY_UNSUPPORTED      = 'UNSUPPORTED selection.modify()';
var SELALLCHILDREN_UNSUPPORTED = 'UNSUPPORTED selection.selectAllChildren()';

// Output string for unsupported functions
// (returning bool 'false' as opposed to throwing an exception)
var UNSUPPORTED = '<i>false</i> (UNSUPPORTED)';

// HTML comparison result contants.
var VALRESULT_NOT_RUN                = 0;  // test hasn't been run yet
var VALRESULT_SETUP_EXCEPTION        = 1;
var VALRESULT_EXECUTION_EXCEPTION    = 2;
var VALRESULT_VERIFICATION_EXCEPTION = 3;
var VALRESULT_UNSUPPORTED            = 4;
var VALRESULT_CANARY                 = 5;  // HTML changes bled into the canary.
var VALRESULT_DIFF                   = 6;
var VALRESULT_ACCEPT                 = 7;  // HTML technically correct, but not ideal.
var VALRESULT_EQUAL                  = 8;

var VALOUTPUT = [  // IMPORTANT: this array MUST be coordinated with the values above!!
    {css: 'grey',        output: '???',    title: 'The test has not been run yet.'},                                            // VALRESULT_NOT_RUN
    {css: 'exception',   output: 'EXC.',   title: 'Exception was thrown during setup.'},                                        // VALRESULT_SETUP_EXCEPTION
    {css: 'exception',   output: 'EXC.',   title: 'Exception was thrown during execution.'},                                    // VALRESULT_EXECUTION_EXCEPTION
    {css: 'exception',   output: 'EXC.',   title: 'Exception was thrown during result verification.'},                          // VALRESULT_VERIFICATION_EXCEPTION
    {css: 'unsupported', output: 'UNS.',   title: 'Unsupported command or value'},                                              // VALRESULT_UNSUPPORTED
    {css: 'canary',      output: 'CANARY', title: 'The command affected the contentEditable root element, or outside HTML.'},   // VALRESULT_CANARY
    {css: 'fail',        output: 'FAIL',   title: 'The result differs from the expectation(s).'},                               // VALRESULT_DIFF
    {css: 'accept',      output: 'ACC.',   title: 'The result is technically correct, but sub-optimal.'},                       // VALRESULT_ACCEPT
    {css: 'pass',        output: 'PASS',   title: 'The test result matches the expectation.'}                                   // VALRESULT_EQUAL
]

// Selection comparison result contants.
var SELRESULT_NOT_RUN = 0;  // test hasn't been run yet
var SELRESULT_CANARY  = 1;  // selection escapes the contentEditable element
var SELRESULT_DIFF    = 2;
var SELRESULT_NA      = 3;
var SELRESULT_ACCEPT  = 4;  // Selection is acceptable, but not ideal.
var SELRESULT_EQUAL   = 5;

var SELOUTPUT = [  // IMPORTANT: this array MUST be coordinated with the values above!!
    {css: 'grey',   output: 'grey',   title: 'The test has not been run yet.'},                           // SELRESULT_NOT_RUN
    {css: 'canary', output: 'CANARY', title: 'The selection escaped the contentEditable boundary!'},      // SELRESULT_CANARY
    {css: 'fail',   output: 'FAIL',   title: 'The selection differs from the expectation(s).'},           // SELRESULT_DIFF   
    {css: 'na',     output: 'N/A',    title: 'The correctness of the selection could not be verified.'},  // SELRESULT_NA     
    {css: 'accept', output: 'ACC.',   title: 'The selection is technically correct, but sub-optimal.'},   // SELRESULT_ACCEPT 
    {css: 'pass',   output: 'PASS',   title: 'The selection matches the expectation.'}                    // SELRESULT_EQUAL  
];

// RegExp for selection markers
var SELECTION_MARKERS = /[\[\]\{\}\|\^]/;

// Special attributes used to mark selections within elements that otherwise
// have no children. Important: attribute name MUST be lower case!
var ATTRNAME_SEL_START = 'bsselstart';
var ATTRNAME_SEL_END   = 'bsselend';

// DOM node type constants.
var DOM_NODE_TYPE_ELEMENT = 1;
var DOM_NODE_TYPE_TEXT    = 3;
var DOM_NODE_TYPE_COMMENT = 8;

// Test parameter names
var PARAM_DESCRIPTION           = 'desc';
var PARAM_PAD                   = 'pad';
var PARAM_EXECCOMMAND           = 'command';
var PARAM_FUNCTION              = 'function';
var PARAM_QUERYCOMMANDSUPPORTED = 'qcsupported';
var PARAM_QUERYCOMMANDENABLED   = 'qcenabled';
var PARAM_QUERYCOMMANDINDETERM  = 'qcindeterm';
var PARAM_QUERYCOMMANDSTATE     = 'qcstate';
var PARAM_QUERYCOMMANDVALUE     = 'qcvalue';
var PARAM_VALUE                 = 'value';
var PARAM_EXPECTED              = 'expected';
var PARAM_EXPECTED_OUTER        = 'expOuter';
var PARAM_ACCEPT                = 'accept';
var PARAM_ACCEPT_OUTER          = 'accOuter';
var PARAM_CHECK_ATTRIBUTES      = 'checkAttrs';
var PARAM_CHECK_STYLE           = 'checkStyle';
var PARAM_CHECK_CLASS           = 'checkClass';
var PARAM_CHECK_ID              = 'checkID';
var PARAM_STYLE_WITH_CSS        = 'styleWithCSS';

// ID suffixes for the output columns
var IDOUT_TR         = '_:TR:';   // per container
var IDOUT_TESTID     = '_:tid';   // per test
var IDOUT_COMMAND    = '_:cmd';   // per test
var IDOUT_VALUE      = '_:val';   // per test
var IDOUT_CHECKATTRS = '_:att';   // per test
var IDOUT_CHECKSTYLE = '_:sty';   // per test
var IDOUT_CONTAINER  = '_:cnt:';  // per container
var IDOUT_STATUSVAL  = '_:sta:';  // per container
var IDOUT_STATUSSEL  = '_:sel:';  // per container
var IDOUT_PAD        = '_:pad';   // per test
var IDOUT_EXPECTED   = '_:exp';   // per test
var IDOUT_ACTUAL     = '_:act:';  // per container

// Output strings to use for yes/no/NA
var OUTSTR_YES = '&#x25CF;'; 
var OUTSTR_NO  = '&#x25CB;'; 
var OUTSTR_NA  = '-';

// Tags at the start of HTML strings where they were taken from
var HTMLTAG_BODY  = 'B:';
var HTMLTAG_OUTER = 'O:';
var HTMLTAG_INNER = 'I:';

// What to use for the canary
var CANARY = 'CAN<br>ARY';

// Containers for tests, and their associated DOM elements:
// iframe, win, doc, body, elem
var containers = [
  { id:       'dM',
    iframe:   null,
    win:      null,
    doc:      null,
    body:     null,
    editor:   null,
    tagOpen:  '<body>',
    tagClose: '</body>',
    editorID: null,
    canary:   '',
  },
  { id:       'body',
    iframe:   null,
    win:      null,
    doc:      null,
    body:     null,
    editor:   null,
    tagOpen:  '<body contenteditable="true">',
    tagClose: '</body>',
    editorID: null,
    canary:   ''
  },
  { id:       'div',
    iframe:   null,
    win:      null,
    doc:      null,
    body:     null,
    editor:   null,
    tagOpen:  '<div contenteditable="true" id="editor-div">',
    tagClose: '</div>',
    editorID: 'editor-div',
    canary:   CANARY
  }
];

// Helper variables to use in test functions
var win = null;     // window object to use for test functions
var doc = null;     // document object to use for test functions
var body = null;    // The <body> element of the current document
var editor = null;  // The contentEditable element (i.e., the <body> or <div>)
var sel = null;     // The current selection after the pad is set up

// Canonicalization emit flags for various purposes
var emitFlagsForCanary = { 
    emitAttrs:         true,
    emitStyle:         true,
    emitClass:         true,
    emitID:            true,
    lowercase:         true,
    canonicalizeUnits: true
};
var emitFlagsForOutput = { 
    emitAttrs:         true,
    emitStyle:         true,
    emitClass:         true,
    emitID:            true,
    lowercase:         false,
    canonicalizeUnits: false
};

// Shades of output colors
var colorShades = ['Lo', 'Hi'];

// Classes of tests
var testClassIDs = ['Finalized', 'RFC', 'Proposed'];
var testClassCount = testClassIDs.length;

// Dictionary storing the detailed test results.
var results = {
    count: 0,
    score: 0
};

// Results - populated by the fillResults() function.
var beacon = [];

// "compatibility" between Python and JS for test quines
var True = true;
var False = false;
