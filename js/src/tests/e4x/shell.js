/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (section, msg)
{
    msg = inSection(section)+"\n"+msg;
    var lines = msg.split ("\n");
    for (var i=0; i<lines.length; i++)
        print (FAILED + lines[i]);
}

function START(summary)
{
    SUMMARY = summary;
    printStatus(summary);
}

function TEST(section, expected, actual)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
   
    SECTION = section;
    EXPECTED = expected;
    ACTUAL = actual;

    return reportCompare(expected, actual, inSection(section) + SUMMARY);
}

function TEST_XML(section, expected, actual)
{
    var actual_t = typeof actual;
    var expected_t = typeof expected;

    SECTION = section;
    EXPECTED = expected;
    ACTUAL = actual;

    if (actual_t != "xml") {
        // force error on type mismatch
        return TEST(section, new XML(), actual);
    }
 
    if (expected_t == "string") {
        return TEST(section, expected, actual.toXMLString());
    }

    if (expected_t == "number") {
        return TEST(section, String(expected), actual.toXMLString());
    }
 
    throw section + ": Bad TEST_XML usage: type of expected is " +
        expected_t + ", should be number or string";

    // suppress warning
    return false;
}

function SHOULD_THROW(section)
{
    TEST(section, "exception", "no exception");
}

function END()
{
}

if (typeof options == 'function')
{
  options('moar_xml');
}
