/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("Qualified names specifying all names in no namespace should only match names without namespaces");
printBugNumber(263935);

var ns1 = new Namespace("http://www.ns1.com");
var ns2 = new Namespace("http://www.ns2.com");
var none = new Namespace();

var x = <x/>
x.foo = "one";
x.ns1::foo = "two";
x.ns2::foo = "three";
x.bar = "four";

var actual = x.none::*;

var expected =
<>
  <foo>one</foo>
  <bar>four</bar>
</>

TEST(1, expected, actual);

END();
