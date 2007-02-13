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

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "Clean AS2";  // Version of JavaScript or ECMA
var TITLE   = "Variable";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


    // <empty> Identifier = <empty>
    //id; // runtime error

    // <empty> Identifier = AssignmentExpression
    idAssignExpr = true;

    // <empty> VariableBindingList, Identifier = <empty>
    //id1, id2, id3; // runtime errors

    // var VariableBindingList, Identifier = AssignmentExpression
    id1AssignExpr = true, id2AssignExpr = false, id3AssignExpr = true;
    //id1AssignExprB, id2AssignExprB, id3AssignExprB = true; // runtime errors



AddTestCase( "Variable Definition <empty> defined inside class", 1, 1);

AddTestCase( "var Identifier = <empty>", "id", (id = "id", id));
AddTestCase( "var Identifier = AssignmentExpression", true, idAssignExpr); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [1]", true, (id1 = true, id1)); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [2]", true, (id2 = true, id2)); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [3]", true, (id3 = true, id3)); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [1]", true, id1AssignExpr); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [2]", false, id2AssignExpr); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [3]", true, id3AssignExpr); 
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [4]", undefined, id1AssignExprB); 
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [5]", undefined, id2AssignExprB); 
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [6]", true, id3AssignExprB); 


test();       // leave this alone.  this executes the test cases and
              // displays results.
