// |reftest| pref(javascript.options.xml.content,true) fails
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.2.1 - XML.toXMLString");

var BUGNUMBER = 297025;

printBugNumber(BUGNUMBER);

var n = 1;
var actual;
var expect;


// Semantics

var xml;

// text 10.2.1 - Step 4

printStatus(inSection(n++) + ' testing text');

var text = 'this is text';


printStatus(inSection(n++) + ' testing text with pretty printing');
XML.prettyPrinting = true;
XML.prettyIndent   = 10;
xml = new XML(text);
expect = text;
actual = xml.toXMLString();
TEST(n, expect, actual);

printStatus(inSection(n++) + ' testing text with whitespace with pretty printing');
XML.prettyPrinting = true;
XML.prettyIndent   = 10;
xml = new XML(' \t\r\n' + text + ' \t\r\n');
expect = text;
actual = xml.toXMLString();
TEST(n, expect, actual);

printStatus(inSection(n++) + ' testing text with whitespace without pretty printing');
XML.prettyPrinting = false;
xml = new XML(' \t\r\n' + text + ' \t\r\n');
expect = ' \t\r\n' + text + ' \t\r\n';
actual = xml.toXMLString();
TEST(n, expect, actual);

// EscapeElementValue - 10.2.1 Step 4.a.ii

printStatus(inSection(n++) + ' testing text EscapeElementValue');
var alpha = 'abcdefghijklmnopqrstuvwxyz';
xml = <a>{"< > &"}{alpha}</a>;
xml = xml.text();
expect = '&lt; &gt; &amp;' + alpha
actual = xml.toXMLString();
TEST(n, expect, actual);

// attribute, EscapeAttributeValue - 10.2.1 Step 5

printStatus(inSection(n++) + ' testing attribute EscapeAttributeValue');
xml = <foo/>;
xml.@bar = '"<&\u000A\u000D\u0009' + alpha;
expect = '<foo bar="&quot;&lt;&amp;&#xA;&#xD;&#x9;' + alpha + '"/>';
actual = xml.toXMLString();
TEST(n, expect, actual);

// Comment - 10.2.1 Step 6

printStatus(inSection(n++) + ' testing Comment');
XML.ignoreComments = false;
xml = <!-- Comment -->;
expect = '<!-- Comment -->';
actual = xml.toXMLString();
TEST(n, expect, actual);


// Processing Instruction - 10.2.1 Step 7

printStatus(inSection(n++) + ' testing Processing Instruction');
XML.ignoreProcessingInstructions = false;
xml = <?pi value?>;
expect = '<?pi value?>';
actual = xml.toXMLString();
TEST(n, expect, actual);

// 10.2.1 Step 8+

// From Martin and Brendan
var link;
var xlinkNamespace;

printStatus(inSection(n++));
expect = '<link xlink:type="simple" xmlns:xlink="http://www.w3.org/1999/xlink"/>';

link = <link type="simple" />;
xlinkNamespace = new Namespace('xlink', 'http://www.w3.org/1999/xlink');
link.addNamespace(xlinkNamespace);
printStatus('In scope namespace: ' + link.inScopeNamespaces());
printStatus('XML markup: ' + link.toXMLString());
link.@type.setName(new QName(xlinkNamespace, 'type'));
printStatus('name(): ' + link.@*::*[0].name());
actual = link.toXMLString();
printStatus(actual);

TEST(n, expect, actual);

printStatus(inSection(n++));
link = <link type="simple" />;
xlinkNamespace = new Namespace('xlink', 'http://www.w3.org/1999/xlink');
printStatus('XML markup: ' + link.toXMLString());
link.@type.setNamespace(xlinkNamespace);
printStatus('name(): ' + link.@*::*[0].name());
actual = link.toXMLString();
printStatus(actual);

TEST(n, expect, actual);

END();
