// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 407323;
var summary = 'XML, XMLList, QName are mutable, Namespace is not.';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var obj           = {};
  var saveQName     = QName;
  var saveXML       = XML;
  var saveXMLList   = XMLList;
  var saveNamespace = Namespace;

  QName = obj;
  reportCompare(obj, QName, summary + ': QName');

  XML = obj;
  reportCompare(obj, XML, summary + ': XML');

  XMLList = obj;
  reportCompare(obj, XMLList, summary + ': XMLList');

  Namespace = obj;
  reportCompare(obj, Namespace, summary + ': Namespace');

  exitFunc ('test');
}
