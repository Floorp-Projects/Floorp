// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Make XPConnect refuse to wrap e4x";
var BUGNUMBER = 327697;
var actual = 'No Hang';
var expect = 'No Hang';

printBugNumber(BUGNUMBER);
START(summary);
printStatus('This test runs in the browser only');

function init()
{
  try
  {
      var sel = document.getElementsByTagName("select")[0];
      sel.add(document.createElement('foo'), <bar/>);
  }
  catch(ex)
  {
      printStatus(ex + '');
  }
  TEST(1, expect, actual);
  END();
  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
}

if (typeof window != 'undefined')
{
    // delay test driver end
    gDelayTestDriverEnd = true;

    document.write('<select></select>');
    window.addEventListener("load", init, false);
}
else
{
    TEST(1, expect, actual);
    END();
}

