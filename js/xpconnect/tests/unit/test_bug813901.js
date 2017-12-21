/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=813901 */

const Cu = Components.utils;

// Make sure that we can't inject __exposedProps__ via the proto of a COW-ed object.

function checkThrows(expression, sb, regexp) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + '; return "allowed"; } catch (e) { return e.toString(); }})();', sb);
  dump('result: ' + result + '\n\n\n');
  Assert.ok(!!regexp.exec(result));
}

function run_test() {

  var sb = new Cu.Sandbox('http://www.example.org');
  sb.obj = {foo: 2};
  checkThrows('obj.foo = 3;', sb, /denied/);
  Cu.evalInSandbox("var p = {__exposedProps__: {foo: 'rw'}};", sb);
  sb.obj.__proto__ = sb.p;
  checkThrows('obj.foo = 4;', sb, /denied/);
}
