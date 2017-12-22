/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 291213;
var summary = 'Do not crash in args_resolve enumerating |arguments|';
var actual = 'No Crash';
var expect = 'No Crash';

var scriptCode = "var result = \"\" + arguments; " +
  "for (i in arguments) " +
  "result += \"\\\n  \" + i + \" \" + arguments[i]; result;";
var scripts = {};

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  scripts["A"] = new Script(scriptCode);

  scripts["B"] = (function() {
                    return new Script(scriptCode);
                  })();

  scripts["C"] = (function() {
                    function x() { "a"; }
                    return new Script(scriptCode);
                  })();

// any Object (window, document, new Array(), ...)
  var anyObj = new Object();
  scripts["D"] = (function() {
                    function x() { anyObj; }
                    return new Script(scriptCode);
                  })();

  var result;
  for (var i in scripts) {
    try { result = scripts[i].exec(); }
    catch (e) { result = e; }
    printStatus(i + ") " + result);
  }
}
reportCompare(expect, actual, summary);
