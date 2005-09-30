/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bob Clary
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Spider hook function to execute ecma*, js* browser based JS tests. 
 */


var gCheckInterval = 1000;

// if jsUnit status doesn't change, force page complete.

function userOnBeforePage()
{
  dlog('userOnBeforePage');

}

var gReport;

function userOnAfterPage()
{
  dlog('userOnAfterPage');
  var win = gSpider.mDocument.defaultView;
  if (win.wrappedJSObject)
  {
    win = win.wrappedJSObject;
  }
  win.__Report = win.reportHTML;
  win.reportHTML = function () { win.__Report(); gPageCompleted = true; };

  win.reportCallBack = function (testwin) 
    {
      if (testwin.wrappedJSObject)
      {
        testwin = testwin.wrappedJSObject;
      }
      var testcases = testwin.testcases;
      for (var i = 0; i < testcases.length; i++)
      {
        var testcase = testcases[i];
        cdump('testname: '    + testcase.name + ' ' + 
              'bug: '         + testcase.bugnumber + ' ' + 
              (testcase.passed ? 'PASSED':'FAILED') + ' ' +
              'description: ' + testcase.description + ' ' + 
              'expected: '    + testcase.expect + ' ' + 
              'actual: '      + testcase.actual + ' ' +
              'reason: '      + testcase.reason);
      }
    };

// only report failures
  win.document.forms.testCases.failures.checked = true;
// these calls are all async
  win.selectAll('ecma');
  win.selectAll('ecma_2');
  win.selectAll('ecma_3');
  win.selectAll('js1_1');
  win.selectAll('js1_2');
  win.selectAll('js1_3');
  win.selectAll('js1_4');
  win.selectAll('js1_5');
// so need to delay this call to make 
// sure the previous ones have completed
  win.setTimeout("executeList()", 10000);
}


gConsoleListener.onConsoleMessage = 
function userOnConsoleMessage(s)
{
  dump(s);
};
