/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This unit test checks that our string formatter works with different patterns and
 * arguments.
 * Initially copied from unit tests at https://github.com/alexei/sprintf.js
 */

const { sprintf } = require("devtools/shared/sprintfjs/sprintf");
const PI = 3.141592653589793;

function run_test() {
  // Simple patterns
  equal("%", sprintf("%%"));
  equal("10", sprintf("%b", 2));
  equal("A", sprintf("%c", 65));
  equal("2", sprintf("%d", 2));
  equal("2", sprintf("%i", 2));
  equal("2", sprintf("%d", "2"));
  equal("2", sprintf("%i", "2"));
  equal('{"foo":"bar"}', sprintf("%j", { foo: "bar" }));
  equal('["foo","bar"]', sprintf("%j", ["foo", "bar"]));
  equal("2e+0", sprintf("%e", 2));
  equal("2", sprintf("%u", 2));
  equal("4294967294", sprintf("%u", -2));
  equal("2.2", sprintf("%f", 2.2));
  equal("3.141592653589793", sprintf("%g", PI));
  equal("10", sprintf("%o", 8));
  equal("%s", sprintf("%s", "%s"));
  equal("ff", sprintf("%x", 255));
  equal("FF", sprintf("%X", 255));
  equal(
    "Polly wants a cracker",
    sprintf("%2$s %3$s a %1$s", "cracker", "Polly", "wants")
  );
  equal("Hello world!", sprintf("Hello %(who)s!", { who: "world" }));
  equal("true", sprintf("%t", true));
  equal("t", sprintf("%.1t", true));
  equal("true", sprintf("%t", "true"));
  equal("true", sprintf("%t", 1));
  equal("false", sprintf("%t", false));
  equal("f", sprintf("%.1t", false));
  equal("false", sprintf("%t", ""));
  equal("false", sprintf("%t", 0));

  equal("undefined", sprintf("%T", undefined));
  equal("null", sprintf("%T", null));
  equal("boolean", sprintf("%T", true));
  equal("number", sprintf("%T", 42));
  equal("string", sprintf("%T", "This is a string"));
  equal("function", sprintf("%T", Math.log));
  equal("array", sprintf("%T", [1, 2, 3]));
  equal("object", sprintf("%T", { foo: "bar" }));

  equal("regexp", sprintf("%T", /<("[^"]*"|"[^"]*"|[^"">])*>/));

  equal("true", sprintf("%v", true));
  equal("42", sprintf("%v", 42));
  equal("This is a string", sprintf("%v", "This is a string"));
  equal("1,2,3", sprintf("%v", [1, 2, 3]));
  equal("[object Object]", sprintf("%v", { foo: "bar" }));
  equal(
    "/<(\"[^\"]*\"|'[^']*'|[^'\">])*>/",
    sprintf("%v", /<("[^"]*"|'[^']*'|[^'">])*>/)
  );

  // sign
  equal("2", sprintf("%d", 2));
  equal("-2", sprintf("%d", -2));
  equal("+2", sprintf("%+d", 2));
  equal("-2", sprintf("%+d", -2));
  equal("2", sprintf("%i", 2));
  equal("-2", sprintf("%i", -2));
  equal("+2", sprintf("%+i", 2));
  equal("-2", sprintf("%+i", -2));
  equal("2.2", sprintf("%f", 2.2));
  equal("-2.2", sprintf("%f", -2.2));
  equal("+2.2", sprintf("%+f", 2.2));
  equal("-2.2", sprintf("%+f", -2.2));
  equal("-2.3", sprintf("%+.1f", -2.34));
  equal("-0.0", sprintf("%+.1f", -0.01));
  equal("3.14159", sprintf("%.6g", PI));
  equal("3.14", sprintf("%.3g", PI));
  equal("3", sprintf("%.1g", PI));
  equal("-000000123", sprintf("%+010d", -123));
  equal("______-123", sprintf("%+'_10d", -123));
  equal("-234.34 123.2", sprintf("%f %f", -234.34, 123.2));

  // padding
  equal("-0002", sprintf("%05d", -2));
  equal("-0002", sprintf("%05i", -2));
  equal("    <", sprintf("%5s", "<"));
  equal("0000<", sprintf("%05s", "<"));
  equal("____<", sprintf("%'_5s", "<"));
  equal(">    ", sprintf("%-5s", ">"));
  equal(">0000", sprintf("%0-5s", ">"));
  equal(">____", sprintf("%'_-5s", ">"));
  equal("xxxxxx", sprintf("%5s", "xxxxxx"));
  equal("1234", sprintf("%02u", 1234));
  equal(" -10.235", sprintf("%8.3f", -10.23456));
  equal("-12.34 xxx", sprintf("%f %s", -12.34, "xxx"));
  equal('{\n  "foo": "bar"\n}', sprintf("%2j", { foo: "bar" }));
  equal('[\n  "foo",\n  "bar"\n]', sprintf("%2j", ["foo", "bar"]));

  // precision
  equal("2.3", sprintf("%.1f", 2.345));
  equal("xxxxx", sprintf("%5.5s", "xxxxxx"));
  equal("    x", sprintf("%5.1s", "xxxxxx"));

  equal(
    "foobar",
    sprintf("%s", function() {
      return "foobar";
    })
  );
}
