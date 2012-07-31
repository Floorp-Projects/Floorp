// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// testcase from  Martin.Honnen@arcor.de

var summary = 'call setNamespace on element with already existing default namespace';
var BUGNUMBER = 277779;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var xhtml2NS = new Namespace('http://www.w3.org/2002/06/xhtml2');
var xml = <html xmlns="http://www.w3.org/1999/xhtml" />;
xml.setNamespace(xhtml2NS);


expect = 'http://www.w3.org/2002/06/xhtml2';
actual = xml.namespace().toString();
TEST(1, expect, actual);

END();
