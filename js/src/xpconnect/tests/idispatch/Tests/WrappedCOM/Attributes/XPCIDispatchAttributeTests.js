/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * \file XPCIDispatchAttributeTests.js
 * Test IDispatch properties and also type conversion
 */

try
{
	// Test object used to test IDispatch property.
    testObject = { one : 1, two : 2, three : 'three' };
    test();
}
catch (e)
{
    reportFailure("Unhandled exception encountered: " + e.toString());
}

/**
 * Main function to perform test
 */
function test()
{
    printStatus("Read-Write Attributes");

    /*
    * These values come from xpctest_attributes.idl and xpctest_attributes.cpp
    */

    o = COMObject(CLSID_nsXPCDispTestProperties);

    // Check the class an interface to make sure they're ok
    reportCompare(
        "object",
        typeof o,
        "typeof COMObject(CLSID_nsXPCDispTestProperties)");
    // Make sure we can read the values
    testProperty("o.Boolean", ["true", "1","testObject", "'T'", "'F'", "'true'","'false'"], false);
    testProperty("o.Char", ["32"], true);
    testProperty("o.COMPtr", ["0"], true);
    testProperty("o.Currency", ["0", "0.5", "10000.00"], true);
    testProperty("o.Date", ["'04/01/03'"], false);
    testProperty("o.DispatchPtr", ["testObject"], false);
    testProperty("o.Double", ["5.555555555555555555555", "5.555555555555555555555", "5", "-5"], true);
    testProperty("o.Float", ["-5.55555555", "5.5555555", "5", "-5"], true);
    testProperty("o.Long", ["-5", "1073741823", "-1073741824", "1073741824", "-1073741825", "5.5"], true);
    testProperty("o.SCode", ["5"], true);
    testProperty("o.Short", ["5", "-5", "32767", "-32768"], true);
    testProperty("o.String", ["'A String'", "'c'", "5", "true"], false);
    testProperty("o.Variant", ["'A Variant String'", "testObject", "10", "5.5"], false);
    
    // Parameterized property tests
    for (index = 0; index < o.ParameterizedPropertyCount; ++index)
        compareExpression(
            "o.ParameterizedProperty(index)",
            index + 1,
            "Reading initial value from parameterized property " + index);
    for (index = 0; index < o.ParameterizedPropertyCount; ++index)
        compareExpression(
            "o.ParameterizedProperty(index) = index + 5",
            index + 5,
            "Assigning parameterized property " + index);

    for (index = 0; index < o.ParameterizedPropertyCount; ++index)
        compareExpression(
            "o.ParameterizedProperty(index)",
            index + 5,
            "Reading new value from parameterized property " + index);
}

/**
 * Tests retrieving a value from a property and setting multiple values on
 * the property
 */
function testProperty(propertyExpression, values, tryString)
{
    print(propertyExpression);
    try
    {
        reportCompare(
            eval(propertyExpression),
            eval(propertyExpression),
            propertyExpression);
    }
    catch (e)
    {
        reportFailure("Testing initial value of " + propertyExpression + " Exception: " + e.toString());
    }
    for (i = 0; i < values.length; ++i)
    {
        var value = values[i];
        var expression = propertyExpression + "=" + value;
        print(expression);
        try
        {
            reportCompare(
                eval(expression),
                eval(value),
                expression);
            if (tryString)
            {
                expression = propertyExpression + "='" + value + "'";
                print(expression);
                reportCompare(
                    eval(expression),
                    eval("'" + value + "'"),
                    expression);
            }
        }
        catch (e)
        {
            reportFailure("Testing assignment: " + expression + " Exception: " + e.toString());
        }
    }
}