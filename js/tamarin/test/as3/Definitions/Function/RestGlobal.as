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

startTest();

function myRest( ... rest ):int{
	return rest.length;
}

AddTestCase ("myRest( ... rest) called with zero Args", 0, myRest() );
AddTestCase ("myRest( ... rest) called with one Arg", 1, myRest(1) );
AddTestCase ("myRest( ... rest) called with multiple Args", 4, myRest(1,2,3,4) );
AddTestCase ("myRest( ... rest) called with multiple different Args", 4, myRest(1,"2",false,Number.NaN) );

function foo(){}
var resArray:Array = [foo, new Array().toString(),-1,2,"3",true,undefined,null,Number.NaN];
function myRest2( a:Array, ... cust):Boolean {
	if( cust.length > 0 ){
		for( var i:int = 0; i < cust.length; i++ ){
			AddTestCase( "myRest2( a:Array, ... cust)", resArray[i], cust[i] );
		}
		return true;
	} else {
		return false;
	}
}

if( !myRest2( new Array(),foo, new Array().toString(),-1,2,"3",true,undefined,null,Number.NaN )){
	AddTestCase("ERROR", 0, 1);
}


test();



