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
var TITLE   = "Static Variable Definition";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


import VarDefStatic.*;
 
var VARDEFEMPTY = new VarDefStaticClass();
AddTestCase( "Static Variable Definition defined inside class", 1, 1);

AddTestCase( "var Identifier = <empty>", "id", (VARDEFEMPTY.setid("id"), VARDEFEMPTY.getid()));
AddTestCase( "var Identifier : TypeExpression = <empty>", true, (VARDEFEMPTY.setidTypeExpr(true), 
                                                                 VARDEFEMPTY.getidTypeExpr()));
AddTestCase( "var Identifier = AssignmentExpression", true, VARDEFEMPTY.getidAssignExpr()); 
AddTestCase( "var Identifier : TypeExpression = AssignmentExpression", true, VARDEFEMPTY.getidTypeExprAssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [1]", true, (VARDEFEMPTY.setid1(true), 
                                                                         VARDEFEMPTY.getid1()));
AddTestCase( "var VariableBindingList, Identifier = <empty> [2]", true, (VARDEFEMPTY.setid2(true), 
                                                                         VARDEFEMPTY.getid2()));
AddTestCase( "var VariableBindingList, Identifier = <empty> [3]", true, (VARDEFEMPTY.setid3(true), 
                                                                         VARDEFEMPTY.getid3()));
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [1]", true, (VARDEFEMPTY.setid1TypeExpr(true), 
                                                                                          VARDEFEMPTY.getid1TypeExpr()));
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [2]", true, (VARDEFEMPTY.setid2TypeExpr(true), 
                                                                                          VARDEFEMPTY.getid2TypeExpr()));
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [3]", true, (VARDEFEMPTY.setid3TypeExpr(true), 
                                                                                          VARDEFEMPTY.getid3TypeExpr()));
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [1]", true, VARDEFEMPTY.getid1AssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [2]", false, VARDEFEMPTY.getid2AssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [3]", true, VARDEFEMPTY.getid3AssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [4]", undefined, VARDEFEMPTY.getid1AssignExprB()); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [5]", undefined, VARDEFEMPTY.getid2AssignExprB()); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [6]", true, VARDEFEMPTY.getid3AssignExprB()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [1]", true, VARDEFEMPTY.getid1TypeExprAssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [2]", false, VARDEFEMPTY.getid2TypeExprAssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [3]", true, VARDEFEMPTY.getid3TypeExprAssignExpr()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [4]", false, VARDEFEMPTY.getid1TypeExprAssignExprB()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [5]", false, VARDEFEMPTY.getid2TypeExprAssignExprB()); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [6]", true, VARDEFEMPTY.getid3TypeExprAssignExprB()); 
AddTestCase( "var Identifier, Identifier : TypeExpression = <empty> [1]", true, (VARDEFEMPTY.setidA(true), 
                                                                                 VARDEFEMPTY.getidA()));
AddTestCase( "var Identifier, Identifier : TypeExpression = <empty> [2]", true, (VARDEFEMPTY.setidB(true), 
                                                                                 VARDEFEMPTY.getidB()));
AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [1]", false, VARDEFEMPTY.getidAAssign() )
AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [2]", true, VARDEFEMPTY.getidBAssign() )
AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [3]", undefined, VARDEFEMPTY.getidAAssignB() )
AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [4]", true, VARDEFEMPTY.getidBAssignB() )
var arr = new Array(1,2,3);
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [1]", arr, (VARDEFEMPTY.setidTypeExprA(arr),
                                                                                                   VARDEFEMPTY.getidTypeExprA()) );
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [2]", false, (VARDEFEMPTY.setidTypeExprB(false),
                                                                                                     VARDEFEMPTY.getidTypeExprB()) );
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [1]", arr.toString(), VARDEFEMPTY.getidTypeExprAAssign().toString())
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [2]", true, VARDEFEMPTY.getidTypeExprBAssign() )
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [3]", null, VARDEFEMPTY.getidTypeExprAAssignB())
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [4]", true, VARDEFEMPTY.getidTypeExprBAssignB() )
AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [1]", true, (VARDEFEMPTY.setidId(true),
                                                                                                                VARDEFEMPTY.getidId()) );
AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [2]", arr, (VARDEFEMPTY.setidIdTypeExprA(arr),
                                                                                                               VARDEFEMPTY.getidIdTypeExprA()) );
AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [3]", false, (VARDEFEMPTY.setidIdTypeExprB(false),
                                                                                                               VARDEFEMPTY.getidIdTypeExprB()) );
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [1]", false, VARDEFEMPTY.getidIdAssign());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [2]", arr.toString(), VARDEFEMPTY.getidIdTypeExprAAssign().toString());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [3]", true, VARDEFEMPTY.getidIdTypeExprBAssign());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [4]", undefined, VARDEFEMPTY.getidIdAssignB());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [5]", null, VARDEFEMPTY.getidIdTypeExprAAssignB());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [6]", true, VARDEFEMPTY.getidIdTypeExprBAssignB());






test();       // leave this alone.  this executes the test cases and
              // displays results.
