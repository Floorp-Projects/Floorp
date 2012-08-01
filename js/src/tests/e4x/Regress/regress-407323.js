// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'XML, XMLList, QName are mutable, Namespace is not.';
var BUGNUMBER = 407323;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var obj           = {};
var saveQName     = QName;
var saveXML       = XML;
var saveXMLList   = XMLList;
var saveNamespace = Namespace;

QName = obj;
TEST(1, obj, QName);

XML = obj;
TEST(2, obj, XML);

XMLList = obj;
TEST(3, obj, XMLList);

Namespace = obj;
TEST(4, obj, Namespace);

END();
