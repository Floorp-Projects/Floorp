// |reftest| skip slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 407720;
var summary = 'js_FindClassObject causes crashes with getter/setter - Browser only';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

// stop the test after 60 seconds
var start = new Date();

// delay test driver end
gDelayTestDriverEnd = true;
document.write('<iframe onload="onLoad()"><\/iframe>');

function onLoad() 
{

  if ( (new Date() - start) < 60*1000)
  {
    var x = frames[0].Window.prototype;
    x.a = x.b = x.c = 1;
    x.__defineGetter__("HTML document.all class", function() {});
    frames[0].document.all;

    // retry
    frames[0].location = "about:blank";
  }
  else
  {
    actual = 'No Crash';

    reportCompare(expect, actual, summary);
    gDelayTestDriverEnd = false;
    jsTestDriverEnd();
  }
}
