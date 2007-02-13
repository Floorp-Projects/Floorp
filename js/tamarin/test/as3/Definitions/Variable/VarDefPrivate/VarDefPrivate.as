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
package VarDefPrivate {
  public class VarDefPrivate {

    // var Identifier = <empty>
    private var id;

    // var Identifier : TypeExpression = <empty>
    private var idTypeExpr : Boolean;

    // var Identifier = AssignmentExpression
    private var idAssignExpr = true;

    // var Identifier : TypeExpression = AssignmentExpression
    private var idTypeExprAssignExpr : Boolean = true;

    // var VariableBindingList, Identifier = <empty>
    private var id1, id2, id3;

    // var VariableBindingList, Identifier : TypeExpression = <empty>
    private var id1TypeExpr:Boolean, id2TypeExpr:Boolean, id3TypeExpr:Boolean;

    // var VariableBindingList, Identifier = AssignmentExpression
    private var id1AssignExpr = true, id2AssignExpr = false, id3AssignExpr = true;
    private var id1AssignExprB, id2AssignExprB, id3AssignExprB = true;

    // var VariableBindingList, Identifier : TypeExpression = AssignmentExpression
    private var id1TypeExprAssignExpr:Boolean = true, id2TypeExprAssignExpr:Boolean = false, id3TypeExprAssignExpr:Boolean = true;
    private var id1TypeExprAssignExprB:Boolean, id2TypeExprAssignExprB:Boolean, id3TypeExprAssignExprB:Boolean = true;

    // var Identifier, Identifier : TypeExpression
    private var idA, idB:Boolean;

    // var Identifier, Identifier : TypeExpression = AssignmentExpression
    private var idAAssign = false, idBAssign:Boolean = true; 
    private var idAAssignB, idBAssignB:Boolean = true; 

    // var Identifier : TypeExpressionA, Identifier : TypeExpressionB = <empty>
    private var idTypeExprA:Array, idTypeExprB:Boolean; 

    // var Identifier : TypeExpressionA, Identifier : TypeExpressionB = AssignmentExpression 
    private var idTypeExprAAssign : Array = new Array(1,2,3), idTypeExprBAssign : Boolean = true;
    private var idTypeExprAAssignB : Array, idTypeExprBAssignB : Boolean = true;

    // var Identifier, Identifier:TypeExpressionA, Identifier:TypeExpressionB = <empty>
    private var idId, idIdTypeExprA:Array, idIdTypeExprB:Boolean;

    // var Identifier, Identifier:TypeExpressionA, Identifier:TypeExpressionB = AssignmentExpression
    private var idIdAssign = false, idIdTypeExprAAssign:Array = new Array(1,2,3), idIdTypeExprBAssign:Boolean = true;
    private var idIdAssignB, idIdTypeExprAAssignB:Array, idIdTypeExprBAssignB:Boolean = true;

    // *****************************************************************************************
    // access methods - only used for runtime testing
    // *****************************************************************************************

    public function getid() { return id; }
    public function setid(x) { id = x; }
    public function getidTypeExpr():Boolean { return idTypeExpr; }
    public function setidTypeExpr(x:Boolean) { idTypeExpr = x; }
    public function getidAssignExpr() { return idAssignExpr; }
    public function setidAssignExpr(x) { idAssignExpr = x; }
    public function getidTypeExprAssignExpr():Boolean { return idTypeExprAssignExpr; }
    public function setidTypeExprAssignExpr(x:Boolean) { idTypeExprAssignExpr = x; }
    public function getid1() { return id1; }
    public function setid1(x) { id1 = x; }
    public function getid2() { return id2; }
    public function setid2(x) { id2 = x; }
    public function getid3() { return id3; }
    public function setid3(x) { id3 = x; }
    public function getid1TypeExpr():Boolean { return id1TypeExpr; }
    public function setid1TypeExpr(x:Boolean) { id1TypeExpr = x; } 
    public function getid2TypeExpr():Boolean { return id2TypeExpr; }
    public function setid2TypeExpr(x:Boolean) { id2TypeExpr = x; } 
    public function getid3TypeExpr():Boolean { return id3TypeExpr; }
    public function setid3TypeExpr(x:Boolean) { id3TypeExpr = x; } 
    public function getid1AssignExpr() { return id1AssignExpr; }
    public function getid2AssignExpr() { return id2AssignExpr; }
    public function getid3AssignExpr() { return id3AssignExpr; }
    public function getid1AssignExprB() { return id1AssignExprB; }
    public function getid2AssignExprB() { return id2AssignExprB; }
    public function getid3AssignExprB() { return id3AssignExprB; }
    public function getid1TypeExprAssignExpr():Boolean { return id1TypeExprAssignExpr; }
    public function getid2TypeExprAssignExpr():Boolean { return id2TypeExprAssignExpr; }
    public function getid3TypeExprAssignExpr():Boolean { return id3TypeExprAssignExpr; }
    public function getid1TypeExprAssignExprB():Boolean { return id1TypeExprAssignExprB; }
    public function getid2TypeExprAssignExprB():Boolean { return id2TypeExprAssignExprB; }
    public function getid3TypeExprAssignExprB():Boolean { return id3TypeExprAssignExprB; }
    public function getidA() { return idA; }
    public function setidA(x) { idA = x; }
    public function getidB():Boolean { return idB; }
    public function setidB(x:Boolean) { idB = x; }
    public function getidAAssign() { return idAAssign; }
    public function getidBAssign():Boolean { return idBAssign; }
    public function getidAAssignB() { return idAAssignB; }
    public function getidBAssignB():Boolean { return idBAssignB; }
    public function getidTypeExprA() : Array { return idTypeExprA; }
    public function setidTypeExprA(x:Array) { idTypeExprA = x; }
    public function getidTypeExprB() : Boolean { return idTypeExprB; }
    public function setidTypeExprB(x:Boolean) { idTypeExprB = x; }
    public function getidTypeExprAAssign() : Array { return idTypeExprAAssign; }
    public function getidTypeExprBAssign() : Boolean { return idTypeExprBAssign; }
    public function getidTypeExprAAssignB() : Array { return idTypeExprAAssignB; }
    public function getidTypeExprBAssignB() : Boolean { return idTypeExprBAssignB; }
    public function getidId() { return idId; }
    public function setidId(x) { idId = x; }
    public function getidIdTypeExprA() : Array { return idIdTypeExprA; }
    public function setidIdTypeExprA(x:Array) { idIdTypeExprA = x; }
    public function getidIdTypeExprB() : Boolean{ return idIdTypeExprB; }
    public function setidIdTypeExprB(x:Boolean) { idIdTypeExprB = x; }
    public function getidIdAssign() { return idIdAssign; }
    public function getidIdTypeExprAAssign() : Array { return idIdTypeExprAAssign; }
    public function getidIdTypeExprBAssign() : Boolean{ return idIdTypeExprBAssign; }
    public function getidIdAssignB() { return idIdAssignB; }
    public function getidIdTypeExprAAssignB() : Array { return idIdTypeExprAAssignB; }
    public function getidIdTypeExprBAssignB() : Boolean{ return idIdTypeExprBAssignB; }

  }
}

