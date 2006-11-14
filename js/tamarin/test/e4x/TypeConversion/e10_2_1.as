/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Ethan Hugg
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

START("10.2.1 - XML.toXMLString");

// text

var x1  = new XML("abc & 123");
var x0 = x1.toXMLString();
var y0 = "abc &amp; 123";

AddTestCase( "ToXMLString(text)                          :", true, (x0==y0) );

/*---------------------------------------------------------------------------*/
// comment

x1  = new XML("<x><!-- Hello World --></x>");
x0 = x1.toXMLString();
y0 = "<x/>";

AddTestCase( "ToXMLString(comment)                       :", true, (x0==y0) );

/*---------------------------------------------------------------------------*/
// processing instruction

x1  = new XML("<?xml version='1.0'?><x>i</x>");
x0 = x1.toXMLString();
y0 = "<x>i</x>";

AddTestCase( "ToXMLString(processing-instruction)        :", true, (x0==y0) );

/*---------------------------------------------------------------------------*/
// ToXMLString ( x )

XML.ignoreWhitespace = true;

x1 = new XML("<a><b>B</b><c>C</c></a>");

x0 = x1.toXMLString();
y0 = "<a>\n  <b>B</b>\n  <c>C</c>\n</a>";

AddTestCase( "ToXMLString(XML)                           :", true, (x0==y0) );

END();
