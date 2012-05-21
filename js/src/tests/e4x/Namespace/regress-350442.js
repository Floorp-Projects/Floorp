/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 350442;
var summary = 'toXMLString with namespace definitions';
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

expect = false;
actual = false;

try
{
  var t1 = <tag1 xmlns="http://ns1"/>;
  var n2 = new Namespace("http://ns2");
  t1.@n2::a1="a1 from ns2";

  var t1XMLString = t1.toXMLString();

  var attrMatch = /xmlns:([^=]+)="http:\/\/ns2"/.exec(t1XMLString);
  if (!attrMatch)
    throw "No @n2::a1 attribute present";

  var nsRegexp = new RegExp(attrMatch[1] + ':a1="a1 from ns2"');
  if (!nsRegexp.test(t1XMLString))
    throw "No namespace declaration present for @ns2::a1";
}
catch (e)
{
  actual = e;
}

TEST(1, expect, actual);

END();
