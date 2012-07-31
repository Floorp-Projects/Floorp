// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.9 - XMLList descendants()");

TEST(1, true, XMLList.prototype.hasOwnProperty("descendants"));

// From Martin Honnen   

var gods = <gods>
  <god>
    <name>Kibo</name>
  </god>
  <god>
    <name>Xibo</name>
  </god>
</gods>;

var godList = gods.god;

var expect;
var actual;
var node;
var descendants = godList.descendants();

expect = 4;
actual = descendants.length();
TEST(2, expect, actual)

expect = 'nodeKind(): element, name(): name;\n' +
         'nodeKind(): text, name(): null;\n' +
         'nodeKind(): element, name(): name;\n' +
         'nodeKind(): text, name(): null;\n';

actual = '';

for each (var xml in descendants) {
  actual += 'nodeKind(): ' + xml.nodeKind() + ', name(): ' + xml.name() + ';\n';
}
TEST(4, expect, actual);

END();
