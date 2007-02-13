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

var SECTION = "instanceof-003_TypeErrors";
var VERSION = "ECMA_2";
var TITLE   = "instanceof"

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var testcases = getTestCases();
test();

function InstanceOf( object_1, object_2, expect, array, item ) {

	try{
		result = object_1 instanceof object_2;
	} catch (e:TypeError) {
		result = e.toString();
	} finally {
		array[item] = new TestCase(
			SECTION,
			"(" + object_1 + ") instanceof " + object_2,
			expect,
			typeError(result));
	}

}

function getTestCases() {
    var array = new Array();
    var item = 0;
    // RHS of instanceof must be a Class or Function
    var RHSTypeError="TypeError: Error #1040";
    
    //Boolean
    InstanceOf( true, true, RHSTypeError, array, item++ );
    InstanceOf( false, false, RHSTypeError, array, item++ );
    InstanceOf( false, true, RHSTypeError, array, item++ );
    
    InstanceOf( new Boolean(), true, RHSTypeError, array, item++ );
    InstanceOf( new Boolean(true), true, RHSTypeError, array, item++ );
    InstanceOf( new Boolean(false), true, RHSTypeError, array, item++ );
    
    InstanceOf( new Boolean(), false, RHSTypeError, array, item++ );
    InstanceOf( new Boolean(true), false, RHSTypeError, array, item++ );
    InstanceOf( new Boolean(false), false, RHSTypeError, array, item++ );
    
    InstanceOf( new Boolean(), new Boolean(), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(true), new Boolean(), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(false), new Boolean(), RHSTypeError, array, item++ );
    
    InstanceOf( new Boolean(), new Boolean(true), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(true), new Boolean(true), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(false), new Boolean(true), RHSTypeError, array, item++ );
    
    InstanceOf( new Boolean(), new Boolean(false), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(true), new Boolean(false), RHSTypeError, array, item++ );
    InstanceOf( new Boolean(false), new Boolean(false), RHSTypeError, array, item++ );
    
    //Number
    InstanceOf( 4, 3, RHSTypeError, array, item++ );
    InstanceOf( new Number(), 3, RHSTypeError, array, item++ );
    InstanceOf( new Number(4), 3, RHSTypeError, array, item++ );
    InstanceOf( new Number(), new Number(), RHSTypeError, array, item++ );
    InstanceOf( new Number(4), new Number(4), RHSTypeError, array, item++ );
    
    
    //String
    InstanceOf( "test", "test", RHSTypeError, array, item++ );
    InstanceOf( "test", new String("test"), RHSTypeError, array, item++ );
    InstanceOf( "test", new String(), RHSTypeError, array, item++ );
    InstanceOf( "test", new String(""), RHSTypeError, array, item++ );
    InstanceOf( new String(), new String("test"), RHSTypeError, array, item++ );
    InstanceOf( new String(""), new String(), RHSTypeError, array, item++ );
    InstanceOf( new String("test"), new String(""), RHSTypeError, array, item++ );

    return array;
}
