// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var nTest = 0;

START("13.4.4.10 - XML contains()");

TEST(++nTest, true, XML.prototype.hasOwnProperty("contains"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST(++nTest, true, emps.contains(emps));

// Martin - bug 289706

expect = 'gods.contains(\'Kibo\')==false && (gods==\'Kibo\')==false';

var gods = <gods>
  <god>Kibo</god>
  <god>Xibo</god>
</gods>;

printStatus('XML markup is:\r\n' + gods.toXMLString());

var string = 'Kibo';
actual = 'gods.contains(\'' + string + '\')==' + gods.contains(string);
actual += ' && ';
actual += '(gods==\'' + string + '\')==' + (gods == string);

TEST(++nTest, expect, actual);

// Martin - bug 289790

function containsTest(xmlObject, value)
{
    var comparison    = (xmlObject == value);
    var containsCheck = xmlObject.contains(value);
    var result        = (comparison == containsCheck);

    printStatus('Comparing ' + xmlObject.nodeKind() +
                ' against ' + (typeof value) + ':');

    printStatus('==: ' + comparison + '; contains: ' + containsCheck +
                '; check ' + (result ? 'passed' : 'failed'));
    return result;
}

actual = containsTest(new XML('Kibo'), 'Kibo');
TEST(++nTest, true, actual);

actual = containsTest(<god>Kibo</god>, 'Kibo');
TEST(++nTest, true, actual);

END();
