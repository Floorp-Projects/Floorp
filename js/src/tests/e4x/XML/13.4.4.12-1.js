// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "13.4.4.12 - XML Descendants";
var BUGNUMBER = 289117;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var xhtmlMarkup = <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"><head><title>XHTML example</title></head><body><p>Kibology for all</p></body></html>;

expect =  'element:http://www.w3.org/1999/xhtml::head;' +
'element:http://www.w3.org/1999/xhtml::title;' +
'text:XHTML example;' +
'element:http://www.w3.org/1999/xhtml::body;' +
'element:http://www.w3.org/1999/xhtml::p;' +
'text:Kibology for all;';

actual = '';

for each (var descendant in xhtmlMarkup..*) {
    actual += descendant.nodeKind() + ':';
    var name = descendant.name();
    if (name) {
        actual += name;
    }
    else {
        actual += descendant.toString();
    }
    actual += ';'
}

TEST(1, expect, actual);

actual = '';

for each (var descendant in xhtmlMarkup.descendants()) {
    actual += descendant.nodeKind() + ':';
    var name = descendant.name();
    if (name) {
        actual += name;
    }
    else {
        actual += descendant.toString();
    }
    actual += ';'
}

TEST(2, expect, actual);

END();
