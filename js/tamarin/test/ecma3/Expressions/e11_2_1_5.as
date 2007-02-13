/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
    var SECTION = "e11_2_1_5";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Property Accessors";
    writeHeaderToLog( SECTION + " "+TITLE );

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    // go through all Native Function objects, methods, and properties and get their typeof.

    var PROPERTY = new Array();
    var p = 0;

    // try to access properties of primitive types

    PROPERTY[p++] = new Property(  new String("hi"),    "hi",   "hi",   NaN );
    PROPERTY[p++] = new Property(  new Number(NaN),         NaN,    "NaN",    NaN );
    PROPERTY[p++] = new Property(  new Number(3),           3,      "3",    3  );
    PROPERTY[p++] = new Property(  new Boolean(true),        true,      "true",    1 );
    PROPERTY[p++] = new Property(  new Boolean(false),       false,      "false",    0 );

    for ( var i = 0, RESULT; i < PROPERTY.length; i++ ) {
        array[item++] = new TestCase( SECTION,
                                        PROPERTY[i].object + ".valueOf()",
                                        PROPERTY[i].value,
                                        (PROPERTY[i].object.valueOf()));

        array[item++] = new TestCase( SECTION,
                                        PROPERTY[i].object + ".toString()",
                                        PROPERTY[i].string,
                                        (PROPERTY[i].object.toString()));

    }
    return array;
}


function MyObject( value ) {
    this.value = value;
    this.stringValue = value +"";
    this.numberValue = Number(value);
    return this;
}
function Property( object, value, string, number ) {
    this.object = object;
    this.string = String(value);
    this.number = Number(value);
    this.value = value;
}
