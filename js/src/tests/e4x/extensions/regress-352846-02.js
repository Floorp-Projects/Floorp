/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 352846;
var summary = 'Passing unrooted value to OBJ_DEFAULT_VALUE is GC hazard';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var counter = 0;

function prepare_xml()
{
  delete XML.prototype.function::toString;
  Object.defineProperty(Object.prototype, "toString",
                        { get: toSource_getter, enumerable: true,
                          configurable: true });
  return new XML("<a>xml_text</a>");
}

function toSource_getter()
{
  // Make sure that lastInternalResult does not contain prepare_xml
  var tmp = { toSource: function() { return [1,2,3]; } };
  uneval(tmp);

  if (counter++ < 2) return undefined;

  // run GC onr overwrite the heap
  if (typeof gc == "function") gc();
  var x = 1e-100;
  for (var i = 0; i != 50000; ++i)
    var x2 = x / 4;

  // Return a function that would access this in non-trivial way to
  // check if prepare_xml() was rooted.
  return function() {
    print("xxx");
    return this.toXMLString();
  };
}

var a = [1, 2];

a.sort(prepare_xml);

TEST(1, expect, actual);

END();
