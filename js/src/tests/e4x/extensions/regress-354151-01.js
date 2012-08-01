// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 354151;
var summary = 'Bad assumptions about Array elements';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function getter() { return 1; }
function setter(v) { return v; }

var xml = <a xmlns="foo:bar"/>;

Array.prototype.__defineGetter__(0, getter);
Array.prototype.__defineSetter__(0, setter);

xml.namespace();

delete Array.prototype[0];

TEST(1, expect, actual);

END();
