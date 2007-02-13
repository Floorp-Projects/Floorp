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

START("13.2.5 - Properties of Namespace Instances");

n = new Namespace("ns", "http://someuri");
TEST(1, true, n.hasOwnProperty("prefix"));
TEST(2, true, n.hasOwnProperty("uri"));
TEST(3, true, n.propertyIsEnumerable("prefix"));
TEST(4, true, n.propertyIsEnumerable("uri"));

var prefixCount = 0;
var uriCount = 0;
var p;
for(p in n)
{
    if(p == "prefix") prefixCount++;
    if(p == "uri") uriCount++;
}

TEST(5, 1, prefixCount);
TEST(6, 1, uriCount);
TEST(7, "ns", n.prefix);
TEST(8, "http://someuri", n.uri);

var n = new Namespace();
AddTestCase( "Namespace.uri = blank", '', n.uri);

n = new Namespace('http://www.w3.org/TR/html4/');
AddTestCase( "Namespace.uri = http://www.w3.org/TR/html4/", 'http://www.w3.org/TR/html4/', n.uri);

n = new Namespace('pfx','http://www.w3.org/TR/html4/');
AddTestCase( "Namespace.uri = http://www.w3.org/TR/html4/", 'http://www.w3.org/TR/html4/', n.uri);

n = new Namespace('', '');
AddTestCase( "Namespace.uri = ''", '', n.uri);

// Make sure uri is read-only
var thisError = "no error";
try{
    n.uri = "hi";
}catch(e:ReferenceError){
    thisError = e.toString();
}finally{
    AddTestCase( "Trying to prove that Namespace.uri is read only", "ReferenceError: Error #1074", referenceError(thisError));
 }
AddTestCase( "Setting Namespace.uri", '', n.uri);

n = new Namespace('pfx','http://www.w3.org/TR/html4/');

    try{
    n.uri = "hi";
}catch(e:ReferenceError){
    thisError = e.toString();
}finally{
    AddTestCase( "Trying to prove that Namespace.uri is read only", "ReferenceError: Error #1074", referenceError(thisError));
 }

AddTestCase( "Setting Namespace.uri", 'http://www.w3.org/TR/html4/', n.uri);

n = new Namespace();
AddTestCase( "Namespace.prefix = blank", '', n.prefix);

n = new Namespace('http://www.w3.org/TR/html4/');
AddTestCase( "Namespace.prefix = blank", undefined, n.prefix);

n = new Namespace('pfx','http://www.w3.org/TR/html4/');
AddTestCase( "Namespace.prefix = pfx", 'pfx', n.prefix);

n = new Namespace('', '');
AddTestCase( "Namespace.prefix = ''", '', n.prefix);

// Make sure prefix is read-only
try{
    n.prefix = "hi";
}catch(e:ReferenceError){
    thisError = e.toString();
}finally{
    AddTestCase( "Trying to prove that Namespace.prefix is read only", "ReferenceError: Error #1074", referenceError(thisError));
 }
AddTestCase( "Setting Namespace.prefix", '', n.prefix);

n = new Namespace('pfx','http://www.w3.org/TR/html4/');
try{
    n.prefix = "hi";
}catch(e:ReferenceError){
    thisError = e.toString();
}finally{
    AddTestCase( "Trying to prove that Namespace.prefix is read only", "ReferenceError: Error #1074", referenceError(thisError));
 }
AddTestCase( "Setting Namespace.prefix", 'pfx', n.prefix);

n = new Namespace ("http://www.w3.org/XML/1998/namespace");

var myXML = <xliff version="1.0" xml:lang="de">
<file>
<header></header>
<body>
hello
</body>
</file>
</xliff>;

var foo = myXML.@n::lang;

AddTestCase("Using xml:lang namespace", "de", foo.toString());

AddTestCase("Getting name() of xml:lang namespace", "http://www.w3.org/XML/1998/namespace::lang", foo.name().toString());

AddTestCase("Getting uri of xml:lang namespace", "http://www.w3.org/XML/1998/namespace", foo.name().uri);


myXML = <xliff version="1.0" xml:space="false">
<file>
<header></header>
<body>
hello
</body>
</file>
</xliff>;

foo = myXML.@n::space;

AddTestCase("Using xml:space namespace", "false", foo.toString());

AddTestCase("Getting name() of xml:space namespace", "http://www.w3.org/XML/1998/namespace::space", foo.name().toString());

AddTestCase("Getting uri of xml:space namespace", "http://www.w3.org/XML/1998/namespace", foo.name().uri);


namespace foot = "bar";
xn = new Namespace ("p", "y");

var for_in_values = [];
for (var nn in foot)
{
	for_in_values.push(nn);
}

AddTestCase("Prefix in for-in loop", "prefix", for_in_values[1]);
//AddTestCase("Prefix in for-in loop BUG 125319", false, (for_in_values[1] == "prefix"));
AddTestCase("URI in for-in loop", "uri", for_in_values[0]);
//AddTestCase("URI in for-in loop BUG 125316", false, (for_in_values[0] == "uri"));

var for_each_values = [];
for each (nn in foot)
{
	for_each_values.push(nn);
}

AddTestCase("Prefix in for-each loop", "bar", for_each_values[0]);
//AddTestCase("Prefix in for-each loop BUG 125316", false, (for_each_values[0] == "bar"));
AddTestCase("URI in for-each loop", undefined, for_each_values[1]);

for_each_values = [];
for each (nn in xn)
{
	for_each_values.push(nn);
}

AddTestCase("Prefix in for-each loop", "y", for_each_values[0]);
AddTestCase("URI in for-each loop", "p", for_each_values[1]);

END();
