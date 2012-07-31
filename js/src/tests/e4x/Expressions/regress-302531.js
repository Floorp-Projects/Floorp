// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */


var summary = "E4X QuoteString should deal with empty string";
var BUGNUMBER = 302531;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function f(e) {
  return <e {e}="" />;
}

XML.ignoreWhitespace = true;
XML.prettyPrinting = true;

expect = (
    <e foo="" />
    ).toXMLString().replace(/</g, '&lt;');

actual = f('foo').toXMLString().replace(/</g, '&lt;');

TEST(1, expect, actual);

END();
