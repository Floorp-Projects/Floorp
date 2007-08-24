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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Simon Bünzli <zeniko@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function run_test() {
  // converts an object to a JSON string and tests its integrity
  function toJSONString(a) {
    var res = JSON.toString(a);
    if (!JSON.isMostlyHarmless(res))
      throw new SyntaxError("Invalid JSON string: " + res);
    return res;
  }
  
  // ensures that an object can't be converted to a JSON string
  function isInvalidType(a) {
    try {
      JSON.toString(a);
      return false;
    } catch (ex) {
      return ex.name == "TypeError";
    }
  }
  // ensures that a string can't be converted back to a JavaScript object
  function isInvalidSyntax(a) {
    try {
      JSON.fromString(a);
      return false;
    } catch (ex) {
      return ex.name == "SyntaxError";
    }
  }
  
  Components.utils.import("resource://gre/modules/JSON.jsm");
  do_check_eq(typeof(JSON), "object");
  
  // some of the tests are adapted from /testing/mochitest/tests/test_Base.js
  do_check_eq(toJSONString(true), "true");
  do_check_eq(toJSONString(false), "false");
  
  do_check_eq(toJSONString(1), "1");
  do_check_eq(toJSONString(1.23), "1.23");
  do_check_eq(toJSONString(1.23e-45), "1.23e-45");
  
  do_check_true(isInvalidType(Infinity));
  do_check_true(isInvalidType(NaN));
  
  //XXXzeniko: using € instead of \u20ac fails because of encoding issues
  do_check_eq(toJSONString("Foo-Bar \b\t\n\f\r\"\\ \x01\u20ac"),
              '"Foo-Bar \\b\\t\\n\\f\\r\\"\\\\ \\u0001\\u20ac"');
  
  do_check_eq(toJSONString(null), "null");
  do_check_true(isInvalidType(undefined));
  
  do_check_eq(toJSONString([1, "2", 3.3]), '[1,"2",3.3]');
  // duck-typed Array (since we'll never really get something instanceof Array)
  do_check_eq(toJSONString({ 0: 0, 1: "1", 2: -2.2, length: 3 }), '[0,"1",-2.2]');
  
  var obj = { a: 1, b: "2", c: [-3e+30] };
  do_check_eq(toJSONString(obj), '{"a":1,"b":"2","c":[-3e+30]}');
  do_check_eq(JSON.toString(obj, ["b", "c"] /* keys to drop */), '{"a":1}');
  
  do_check_true(isInvalidType(function() { }));
  
  // make sure that toJSONString actually works...
  do_check_eq(toJSONString(obj), JSON.toString(obj));
  
  do_check_eq(JSON.fromString("true"), true);
  do_check_eq(JSON.fromString("false"), false);
  do_check_eq(JSON.fromString("1"), 1);
  do_check_eq(JSON.fromString('"2.2"'), "2.2");
  do_check_eq(JSON.fromString("1.23e-45"), 1.23e-45);
  do_check_true(isInvalidSyntax("NaN"));
  
  do_check_eq(JSON.fromString('"Foo-Bar \\b\\t\\n\\f\\r\\"\\\\ \\u0001\\u20ac"'),
                              "Foo-Bar \b\t\n\f\r\"\\ \x01\u20ac");
  do_check_true(isInvalidSyntax('"multi\nline"'));
  do_check_eq(JSON.fromString("null"), null);
  do_check_true(isInvalidSyntax("."));
  
  var res = JSON.fromString('[1,"2",3.3]');
  do_check_eq(res.length, 3);
  do_check_eq(res[2], 3.3);
  // res is an instance of the sandbox's array
  do_check_false(res instanceof Array);
  
  res = JSON.fromString(toJSONString(obj));
  do_check_eq(res.a, obj.a);
  do_check_eq(res.b, obj.b);
  do_check_eq(res.c.length, obj.c.length);
  do_check_eq(res.c[0], obj.c[0]);
  
  // those would throw on JSON.fromString if there's no object |a|
  do_check_true(JSON.isMostlyHarmless("a"));
  do_check_true(JSON.isMostlyHarmless("a[0]"));
  do_check_true(JSON.isMostlyHarmless('a["alert(\\"P0wn3d!\\");"]'));
  
  do_check_false(JSON.isMostlyHarmless('(function() { alert("P0wn3d!"); })()'));
  do_check_false(JSON.isMostlyHarmless('{ get a() { return "P0wn3d!"; } }'));
}
