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
 
 
import GetSetSameName.*;

  
 var SECTION = "FunctionAccessors";
 var VERSION = "AS3"; 
 var TITLE   = "Function Accessors";
 var BUGNUMBER = "133108, 133828";
 

startTest();

var g = new GetSetSameNameArg();
AddTestCase("Calling getter of same name as argument", 4, g.e);
AddTestCase("Calling setter of same name as argument", 5, (g.e = 5, g.e));


var h = new GetSetSame();


try {
	result = h.GetSetSameName.toString();
} catch (e1) {
	result = "exception";
	
}
// This will have to be changed when bug 133824 is fixed
AddTestCase("Calling getter of same name as package", "1,2,3", result);




try {
	h.GetSetSameName = [7,8,9];

	result = h.GetSetSameName.toString();
} catch (e2) {
	result = "exception";
	
}
// This will have to be changed when bug 133824 is fixed
AddTestCase("Calling setter of same name as package", "7,8,9", result);



try {
	ff = GetSetSameName.GetSetSameName.x.toString();

} catch (e3) {
	ff = "exception";
}

AddTestCase("Calling static getter in matching package/class", "1,2,3", GetSetSameName.GetSetSameName.x.toString());


try { 	
	GetSetSameName.GetSetSameName.x = [4,5,6];
	ff = GetSetSameName.GetSetSameName.x.toString();
} catch (e4) {
	ff = "exception";
}

AddTestCase("Calling static setter in matching package/class", "4,5,6", ff);

test();
