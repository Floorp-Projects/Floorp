/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Operator .. should not implicitly quote its right operand";
var BUGNUMBER = 321547;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function a(){
  var x=<a><b><c>value c</c></b></a>;
  return x..c;
}

actual = a.toString();
expect = 'function a() {\n' +
         '    var x = <a><b><c>value c</c></b></a>;\n' +
         '    return x..c;\n' +
         '}';
actual = actual.replace(/[\n ]+/mg, ' ');
expect = expect.replace(/[\n ]+/mg, ' ');

TEST(1, expect, actual);

actual = String(a.valueOf());
expect = 'function a() {\n' +
         '    var x = <a><b><c>value c</c></b></a>;\n' +
         '    return x..c;\n' +
         '}';
actual = actual.replace(/[\n ]+/mg, ' ');
expect = expect.replace(/[\n ]+/mg, ' ');

TEST(3, expect, actual);

actual = String(a);
expect = 'function a() {\n' +
         '    var x = <a><b><c>value c</c></b></a>;\n' +
         '    return x..c;\n' +
         '}';
actual = actual.replace(/[\n ]+/mg, ' ');
expect = expect.replace(/[\n ]+/mg, ' ');

TEST(4, expect, actual);

actual = a();
expect = <c>value c</c>;
actual = actual.replace(/[\n ]+/mg, ' ');
expect = expect.replace(/[\n ]+/mg, ' ');

TEST(5, expect, actual);

END();
