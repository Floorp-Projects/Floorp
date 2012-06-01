/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SUMMARY = '';
var DESCRIPTION = '';
var EXPECTED = '';
var ACTUAL = '';
var MSG = '';
var SECTION = '';

window.onerror = function (msg, page, line)
{
  optionsPush();

  if (typeof SUMMARY == 'undefined')
  {
    SUMMARY = 'Unknown';
  }
  if (typeof SECTION == 'undefined')
  {
    SECTION = 'Unknown';
  }
  if (typeof DESCRIPTION == 'undefined')
  {
    DESCRIPTION = 'Unknown';
  }
  if (typeof EXPECTED == 'undefined')
  {
    EXPECTED = 'Unknown';
  }

  var testcase = new TestCase("unknown-test-name", SUMMARY + DESCRIPTION +
                              ' Section ' + SECTION, EXPECTED, "error");

  testcase.passed = false;

  testcase.reason = page + ':' + line + ': ' + msg;

  reportFailure(SECTION, msg);

  gDelayTestDriverEnd = false;
  jsTestDriverEnd();

  optionsReset();
};

options('moar_xml');

