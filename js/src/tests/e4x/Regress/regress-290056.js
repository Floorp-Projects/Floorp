// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Dont crash when serializing an XML object where the name ' +
    'of an attribute was changed with setName';

var BUGNUMBER = 290056;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var link = <link type="simple" />;
var xlinkNamespace = new Namespace('xlink', 'http://www.w3.org/1999/xlink');
link.addNamespace(xlinkNamespace);
printStatus('In scope namespace: ' + link.inScopeNamespaces());
printStatus('XML markup: ' + link.toXMLString());
link.@type.setName(new QName(xlinkNamespace.uri, 'type'));
printStatus('name(): ' + link.@*::*[0].name());
printStatus(link.toXMLString());

TEST(1, expect, actual);

END();
