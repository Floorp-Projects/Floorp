/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Blake Kaplan, Bob Clary
 */

gTestfile = 'regress-302097.js';

var summary = 'E4X - Function.prototype.toString should not quote {} ' +
    'attribute values';
var BUGNUMBER = 302097;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function f(k) {
  return <xml k={k}/>;
}

actual = f.toString().replace(/</g, '&lt;');
expect = 'function f(k) {\n    return &lt;xml k={k}/>;\n}';

TEST(1, expect, actual);

END();
