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

var SECTION = "8.6.1 Constructor Methods";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        // Version of ECMAScript or ActionScript 
var TITLE   = "Super Property Access";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


///////////////////////////////////////////////////////////////
// add your tests here
  
import SuperProps.*

var thisError;
var result;

var spb = new SuperBase();
AddTestCase( "sanity check base object 1", "base::staticY", spb.baseProp );
AddTestCase( "sanity check base object 2", "base::dynamicX", spb.getBaseVal("x") );
// currently fails - finds undefined dynamic property instead of fixed property:
AddTestCase( "sanity check base object 3", "base::staticY", spb.getBaseVal("y") );
spb.setBaseVal("dynamicProp","base::dynamicProp");
AddTestCase( "sanity check base object 4", "base::dynamicProp", spb.getBaseVal("dynamicProp") );

var sp = new SuperProps();
AddTestCase( "sanity check derived object 1", "base::staticY", sp.inheritedProp );
AddTestCase( "sanity check derived object 2", "base::staticY", sp.superPropDot );
// superPropIndex returns X because we cannot test for it returning Y at the moment:
AddTestCase( "sanity check derived object 3", "base::dynamicX", sp.superPropIndex );
AddTestCase( "sanity check derived object 4", "base::dynamicX", sp.getDerivedVal("x") );
AddTestCase( "sanity check derived object 5", "base::dynamicX", sp.getSuperVal("x") );
AddTestCase( "sanity check derived object 6", "base::dynamicX", sp.getBaseVal("x") );
// currently fails - finds undefined dynamic property instead of fixed property:
AddTestCase( "sanity check derived object 7", "base::staticY", sp.getDerivedVal("y") );
// currently fails - throws exception instead of finding fixed property:
try {
	result = sp.getSuperVal("y");	// super["y"] *should* find base::staticY
} catch (e) {
	result = referenceError( e.toString() );
} finally {
	AddTestCase( "sanity check derived object 8", "base::staticY", result );
}
// currently fails - finds undefined dynamic property instead of fixed property:
AddTestCase( "sanity check derived object 9", "base::staticY", sp.getBaseVal("y") );
sp.setDerivedVal("x","derived::x");
AddTestCase( "check modified property 1", "base::staticY", sp.inheritedProp );
AddTestCase( "check modified property 2", "derived::x", sp.getDerivedVal("x") );
AddTestCase( "check base property 1", "base::staticY", sp.superPropDot );
AddTestCase( "check base property 2", "derived::x", sp.superPropIndex );
AddTestCase( "check base property 3", "derived::x", sp.getSuperVal("x") );
AddTestCase( "check base property 4", "derived::x", sp.getBaseVal("x") );
//
////////////////////////////////////////////////////////////////

test();       // leave this alone.  this executes the test cases and
              // displays results.
