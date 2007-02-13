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
    var SECTION = "12.6.3-4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The for..in statment";
    var BUGNUMBER="http://scopus.mcom.com/bugsplat/show_bug.cgi?id=344855";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    //  for ( LeftHandSideExpression in Expression )
    //  LeftHandSideExpression:NewExpression:MemberExpression

    var o = new MyObject();
    var result = 0;
    var thisError="no error thrown";
  
    for ( MyObject in o ) {
        result += o[MyObject];
    }
   
    
    array[item++] = new TestCase( SECTION,
        "for ( MyObject in o ) { result += o[MyObject] }",6,result);
        
    
    var value="value";
    var result = 0;

    for ( value in o ) {
        result += o[value];
    }

    array[item++] = new TestCase( SECTION,
        "for ( value in o ) { result += o[value]",
        6,
        result );

    var value = "value";
    var result = 0;
    for ( value in o ) {
        result += o[value];
    }

    array[item++] = new TestCase( SECTION,
        "value = \"value\"; for ( value in o ) { result += o[value]",
        6,
        result );

    var value = 0;
    var result = 0;
    for ( value in o ) {
        result += o[value];
    }

    array[item++] = new TestCase( SECTION,
        "value = 0; for ( value in o ) { result += o[value]",
        6,
        result );

    // this causes a segv

    var ob = { 0:"hello" };
    var result = 0;
    for ( ob[0] in o ) {
        result += o[ob[0]];
    }
    array[item++] = new TestCase( SECTION,
        "ob = { 0:\"hello\" }; for ( ob[0] in o ) { result += o[ob[0]]",
        6,
        result );

    var result = 0;
    for ( ob["0"] in o ) {
        result += o[ob["0"]];
    }
    array[item++] = new TestCase( SECTION,
        "value = 0; for ( ob[\"0\"] in o ) { result += o[o[\"0\"]]",
        6,
        result );

    var result = 0;
    var ob = { value:"hello" };
    for ( ob[value] in o ) {
        result += o[ob[value]];
    }
    array[item++] = new TestCase( SECTION,
        "ob = { 0:\"hello\" }; for ( ob[value] in o ) { result += o[ob[value]]",
        6,
        result );

    var result = 0;
    for ( ob["value"] in o ) {
        result += o[ob["value"]];
    }
    array[item++] = new TestCase( SECTION,
        "value = 0; for ( ob[\"value\"] in o ) { result += o[ob[\"value\"]]",
        6,
        result );

    var result = 0;
    for ( ob.value in o ) {
        result += o[ob.value];
    }
    array[item++] = new TestCase( SECTION,
        "value = 0; for ( ob.value in o ) { result += o[ob.value]",
        6,
        result );

    //  LeftHandSideExpression:NewExpression:MemberExpression [ Expression ]
    //  LeftHandSideExpression:NewExpression:MemberExpression . Identifier
    //  LeftHandSideExpression:NewExpression:new MemberExpression Arguments
    //  LeftHandSideExpression:NewExpression:PrimaryExpression:( Expression )
    //  LeftHandSideExpression:CallExpression:MemberExpression Arguments
    //  LeftHandSideExpression:CallExpression Arguments
    //  LeftHandSideExpression:CallExpression [ Expression ]
    //  LeftHandSideExpression:CallExpression . Identifier

    return array;
}

function MyObject() {
    this.value = 2;
    this[0] = 4;
    return this;
      
}
