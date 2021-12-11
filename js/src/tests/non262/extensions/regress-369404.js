// |reftest| skip-if(xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 369404;
var summary = 'Do not assert: !SPROP_HAS_STUB_SETTER(sprop) || (sprop->attrs & JSPROP_GETTER) ';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  gDelayTestDriverEnd = true;
  document.write('<span id="r"> </span>' +
                 '<script>' +
                 'f = function(){};' +
                 'f.prototype = document.getElementById("r").childNodes;' +
                 'j = new f();' +
                 'j[0] = null;' +
                 '</script>');
  window.addEventListener('load', crash, false);
}

function crash()
{
  gDelayTestDriverEnd = false;
  reportCompare(expect, actual, summary);
  jsTestDriverEnd();
}
