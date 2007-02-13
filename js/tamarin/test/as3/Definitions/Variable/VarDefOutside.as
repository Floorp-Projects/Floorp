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


    // var Identifier = <empty>
    var id;

    // var Identifier : TypeExpression = <empty>
    var idTypeExpr : Boolean;

    // var Identifier = AssignmentExpression
    var idAssignExpr = true;

    // var Identifier : TypeExpression = AssignmentExpression
    var idTypeExprAssignExpr : Boolean = true;

    // var VariableBindingList, Identifier = <empty>
    var id1, id2, id3;

    // var VariableBindingList, Identifier : TypeExpression = <empty>
    var id1TypeExpr:Boolean, id2TypeExpr:Boolean, id3TypeExpr:Boolean;

    // var VariableBindingList, Identifier = AssignmentExpression
	// Bug 117477
    //var id1AssignExpr = true, id2AssignExpr = false, id3AssignExpr = true;
    var id1AssignExprB, id2AssignExprB, id3AssignExprB = true;

    // var VariableBindingList, Identifier : TypeExpression = AssignmentExpression
	// Bug 117477
    //var id1TypeExprAssignExpr:Boolean = true, id2TypeExprAssignExpr:Boolean = false, id3TypeExprAssignExpr:Boolean = true;
    var id1TypeExprAssignExprB:Boolean, id2TypeExprAssignExprB:Boolean, id3TypeExprAssignExprB:Boolean = true;

    // var Identifier, Identifier : TypeExpression
    var idA, idB:Boolean;

    // var Identifier, Identifier : TypeExpression = AssignmentExpression
	// Bug 117477
    //var idAAssign = false, idBAssign:Boolean = true; 
    //var idAAssignB, idBAssignB:Boolean = true; 

    // var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty>
    var idTypeExprA:Array, idTypeExprB:Boolean; 

    // var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression 
	// Bug 117477
    //var idTypeExprAAssign : Array = new Array(1,2,3), idTypeExprBAssign : Boolean = true;
    //var idTypeExprAAssignB : Array, idTypeExprBAssignB : Boolean = true;

    // var Identifier, Identifier:TypeExpressionA, Identifier:TypeExpressionB = <empty>
    var idId, idIdTypeExprA:Array, idIdTypeExprB:Boolean;

    // var Identifier, Identifier:TypeExpressionA, Identifier:TypeExpressionB = AssignmentExpression
	// Bug 117477
    //var idIdAssign = false, idIdTypeExprAAssign:Array = new Array(1,2,3), idIdTypeExprBAssign:Boolean = true;
    //var idIdAssignB, idIdTypeExprAAssignB:Array, idIdTypeExprBAssignB:Boolean = true;

AddTestCase( "Variable Definition <empty> defined inside class", 1, 1);

AddTestCase( "var Identifier = <empty>", "id", (id = "id", id));
AddTestCase( "var Identifier : TypeExpression = <empty>", true, (idTypeExpr = true, idTypeExpr ));
AddTestCase( "var Identifier = AssignmentExpression", true, idAssignExpr); 
AddTestCase( "var Identifier : TypeExpression = AssignmentExpression", true, idTypeExprAssignExpr); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [1]", true, (id1 = true, id1)); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [2]", true, (id2 = true, id2)); 
AddTestCase( "var VariableBindingList, Identifier = <empty> [3]", true, (id3 = true, id3)); 
//AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [1]", true, (id1TypeExpr = true, 
//                                                                                          id1TypeExpr));
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [2]", true, (id2TypeExpr = true, 
                                                                                          id2TypeExpr));
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = <empty> [3]", true, (id3TypeExpr = true, 
                                                                                          id3TypeExpr));
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [1]", true, id1AssignExpr); 
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [2]", false, id2AssignExpr); 
//AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [3]", true, id3AssignExpr); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [4]", undefined, id1AssignExprB); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [5]", undefined, id2AssignExprB); 
AddTestCase( "var VariableBindingList, Identifier = AssignmentExpression [6]", true, id3AssignExprB); 
//AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [1]", true, id1TypeExprAssignExpr); 
//AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [2]", false, id2TypeExprAssignExpr); 
//AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [3]", true, id3TypeExprAssignExpr); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [4]", false, id1TypeExprAssignExprB); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [5]", false, id2TypeExprAssignExprB); 
AddTestCase( "var VariableBindingList, Identifier : TypeExpression = AssignmentExpression [6]", true, id3TypeExprAssignExprB); 
AddTestCase( "var Identifier, Identifier : TypeExpression = <empty> [1]", true, (idA = true, 
                                                                                 idA));
AddTestCase( "var Identifier, Identifier : TypeExpression = <empty> [2]", true, (idB = true, 
                                                                                 idB));
//AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [1]", false, idAAssign );
//AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [2]", true, idBAssign );
//AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [3]", undefined, idAAssignB );
//AddTestCase( "var Identifier, Identifier : TypeExpression = AssignmentExpression [4]", true, idBAssignB );
var arr = new Array(1,2,3);
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [1]", arr, (idTypeExprA = arr,
                                                                                                   idTypeExprA ));
AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [2]", false, (idTypeExprB = false,
                                                                                                     idTypeExprB));
//AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [1]", arr.toString(), idTypeExprAAssign.toString())
//AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [2]", true, idTypeExprBAssign )
//AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [3]", undefined, idTypeExprAAssignB)
//AddTestCase( "var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [4]", true, idTypeExprBAssignB )
/*AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [1]", true, (idId = true,
                                                                                                                idId) );
AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [2]", arr, (idIdTypeExprA = arr,
                                                                                                               idIdTypeExprA) );
AddTestCase( "var Identifier, Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty> [3]", false, (idIdTypeExprB = false,
                                                                                                                 idIdTypeExprB) );
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [1]", false, idIdAssign);
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [2]", arr.toString(), idIdTypeExprAAssign.toString());
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [3]", true, idIdTypeExprBAssign);
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [4]", undefined, idIdAssignB);
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [5]", undefined, idIdTypeExprAAssignB);
AddTestCase( "var Identififer, Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression [6]", true, idIdTypeExprBAssignB);
*/

test();       // leave this alone.  this executes the test cases and
              // displays results.
