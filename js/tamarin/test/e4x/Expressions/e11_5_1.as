/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

START("11.5.1 - Equality Operators");

x1 = <alpha>one</alpha>;
y1 = <alpha>one</alpha>;
TEST(1, true, (x1 == y1) && (y1 == x1));

var myxml:XML = <a>foo</a>;
var str1:String = "foo";
TEST(17, true, myxml.hasSimpleContent());
TEST(18, true, myxml==str1);


// Should return false if comparison is not XML
y1 = "<alpha>one</alpha>";
TEST(2, false, (x1 == y1) || (y1 == x1));

y1 = undefined
TEST(3, false, (x1 == y1) || (y1 == x1));

y1 = null
TEST(4, false, (x1 == y1) || (y1 == x1));

// Should check logical equiv.
x1 = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
y1 = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
TEST(5, true, (x1 == y1) && (y1 == x1));

y1 = <alpha attr1="new value">one<bravo attr2="value2">two</bravo></alpha>;
TEST(6, false, (x1 == y1) || (y1 == x1));

m = new Namespace();
n = new Namespace();
TEST(7, true, m == n);

m = new Namespace("uri");
TEST(8, false, m == n);

n = new Namespace("ns", "uri");
TEST(9, true, m == n);

m = new Namespace(n);
TEST(10, true, m == n);

TEST(11, false, m == null);
TEST(12, false, null == m);

m = new Namespace("ns", "http://anotheruri");
TEST(13, false, m == n);

p = new QName("a");
q = new QName("b");
TEST(14, false, p == q);

q = new QName("a");
TEST(15, true, p == q);

q = new QName("http://someuri", "a");
TEST(16, false, p == q);

q = new QName(null, "a");
TEST(16, false, p == q);

var x1  = new XML("<aa><a>A</a><a>B</a><a>C</a></aa>");
var y0 = new XML("<a><b>A</b><c>B</c></a>");
var y1 = new XML("<aa><a>A</a><a>B</a><a>C</a></aa>");
var y2 = new XML("<bb><b>A</b><b>B</b><b>C</b></bb>");
var y3 = new XML("<aa><a>Dee</a><a>Eee</a><a>Fee</a></aa>");

AddTestCase( "x=XMLList,                y=XML                   :", false, (x1==y0) );
AddTestCase( "x=XMLList,                y=XMLList               :", true,  (x1==y1) );
AddTestCase( "x=XMLList,                y=XMLList               :", false, (x1==y2) );
AddTestCase( "x=XMLList,                y=XMLList               :", false, (x1==y3) );


var xt = new XML("<a><b>text</b></a>");
var xa = new XML("<a attr='ATTR'><b>attribute</b></a>");
var xh = new XML("<a>hasSimpleContent</a>");
var yt = new XML("<a><b>text</b></a>");
var ya = new XML("<a attr='ATTR'><b>attribute</b></a>");
var yh = new XML("<a>hasSimpleContent</a>");

AddTestCase( "x.[[Class]]='text,        y.[[Class]]='text'      :", true,  (xt==yt) );
AddTestCase( "x.[[Class]]='text,        y.[[Class]]='attribute' :", false, (xt==ya.@attr) );
AddTestCase( "x.[[Class]]='text,        y.hasSimpleContent()    :", false, (xt==yh) );

AddTestCase( "x.[[Class]]='attribute,   y.[[Class]]='text'      :", false, (xa.@attr==yt) );
AddTestCase( "x.[[Class]]='attribute,   y.[[Class]]='attribute' :", true,  (xa.@attr==ya.@attr) );
AddTestCase( "x.[[Class]]='attribute,   y.hasSimpleContent()    :", false, (xa.@attr==yh) );

AddTestCase( "x.hasSimpleContent(),     y.[[Class]]='text'      :", false, (xh==yt) );
AddTestCase( "x.hasSimpleContent(),     y.[[Class]]='attribute' :", false, (xh==ya.@attr) );
AddTestCase( "x.hasSimpleContent(),     y.hasSimpleContent()    :", true,  (xh==yh) );


var xqn0  = new QName("n0");
var xqn1  = new QName("ns1","n1");

var yqn00 = new QName("n0");
var yqn01 = new QName("nA");
var yqn10 = new QName("ns1", "n1" );
var yqn11 = new QName("ns1", "nB");
var yqn12 = new QName("nsB","n1" );
var yqn13 = new QName("nsB","nB");

AddTestCase( "QName('n0'),              QName('n0')              :", true,  (xqn0==yqn00) );
AddTestCase( "QName('n0'),              QName('nA')              :", false, (xqn0==yqn01) );
AddTestCase( "QName('n0'),              QName('ns1','n1')        :", false, (xqn0==yqn10) );
AddTestCase( "QName('n0'),              QName('ns1','nB')        :", false, (xqn0==yqn11) );
AddTestCase( "QName('n0'),              QName('nsB','n1')        :", false, (xqn0==yqn12) );
AddTestCase( "QName('n0'),              QName('naB','nB')        :", false, (xqn0==yqn13) );

AddTestCase( "QName('ns1','n1'),        QName('n0')              :", false, (xqn1==yqn00) );
AddTestCase( "QName('ns1','n1'),        QName('nA')              :", false, (xqn1==yqn01) );
AddTestCase( "QName('ns1','n1'),        QName('ns1','n1')        :", true,  (xqn1==yqn10) );
AddTestCase( "QName('ns1','n1'),        QName('ns1','nB')        :", false, (xqn1==yqn11) );
AddTestCase( "QName('ns1','n1'),        QName('nsB','n1')        :", false, (xqn1==yqn12) );
AddTestCase( "QName('ns1','n1'),        QName('nsB','nB')        :", false, (xqn1==yqn13) );


var xns0  = new Namespace();
var xns1  = new Namespace("uri1");
var xns2  = new Namespace("pre2","uri2");

var yns00 = new Namespace();
var yns10 = new Namespace("uri1");
var yns11 = new Namespace("uriB");
var yns20 = new Namespace("pre2","uri2");
var yns21 = new Namespace("pre2","uriC");
var yns22 = new Namespace("preC","uri2");
var yns23 = new Namespace("preC","uriC");


AddTestCase( "Namespace(),              Namespace()              :", true,  (xns0==yns00) );
AddTestCase( "Namespace(),              Namespace('uri1')        :", false, (xns0==yns10) );
AddTestCase( "Namespace(),              Namespace('uriB')        :", false, (xns0==yns11) );
AddTestCase( "Namespace(),              Namespace('pre2','uri2') :", false, (xns0==yns20) );
AddTestCase( "Namespace(),              Namespace('pre2','uriC') :", false, (xns0==yns21) );
AddTestCase( "Namespace(),              Namespace('preC','uri2') :", false, (xns0==yns22) );
AddTestCase( "Namespace(),              Namespace('preC','uriC') :", false, (xns0==yns23) );

AddTestCase( "Namespace('uri1'),        Namespace()              :", false, (xns1==yns00) );
AddTestCase( "Namespace('uri1'),        Namespace('uri1')        :", true,  (xns1==yns10) );
AddTestCase( "Namespace('uri1'),        Namespace('uriB')        :", false, (xns1==yns11) );
AddTestCase( "Namespace('uri1'),        Namespace('pre2','uri2') :", false, (xns1==yns20) );
AddTestCase( "Namespace('uri1'),        Namespace('pre2','uriC') :", false, (xns1==yns21) );
AddTestCase( "Namespace('uri1'),        Namespace('preC','uri2') :", false, (xns1==yns22) );
AddTestCase( "Namespace('uri1'),        Namespace('preC','uriC') :", false, (xns1==yns23) );

AddTestCase( "Namespace('pre2','uri2'), Namespace()              :", false, (xns2==yns00) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('uri1')        :", false, (xns2==yns10) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('uriB')        :", false, (xns2==yns11) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('pre2','uri2') :", true,  (xns2==yns20) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('pre2','uriC') :", false, (xns2==yns21) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('preC','uri2') :", true,  (xns2==yns22) );
AddTestCase( "Namespace('pre2','uri2'), Namespace('preC','uriC') :", false, (xns2==yns23) );


END();
