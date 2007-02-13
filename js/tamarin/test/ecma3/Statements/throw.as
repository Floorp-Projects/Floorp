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

	var SECTION = '12.13';
	var VERSION = 'no version';
    startTest();
	var TITLE = 'Statement:throw';

	writeHeaderToLog('Executing script: throw.as');
	writeHeaderToLog( SECTION + " "+ TITLE);


	var testcases = getTestCases();
	test();
	
function getTestCases() {
    var array = new Array();
    var item = 0;	
	
	var t;
	var z = 1;
	

	try { 
		 if(z == 1) 
			throw "Error 1" 
		 else if(z == 2)
			throw "Error 2"
	    } 
	catch(er) {
	    if(er == "Error 1") 
		t = "Error 1"
	    if(er == "Error 2") 
		t = "Error 2"
} 
			
	
	array[item++] = new TestCase( SECTION, "throw t", "Error 1", t);

       var z = 2;
       var Exception_Value = "no error";

       
       
       function f(Exception_Value){
           if (z==2){
           this.Exception_Value = Exception_Value;
           throw Exception_Value;
           }else{
                return Exception_Value
           } 

       }
       thisError='No Error'
       var Err_msg = "Error!z is equal to 2"
       try{
           f(Err_msg);
       }catch(e){
           thisError=e.toString();
       }finally{
      array[item++] = new TestCase( SECTION, "throw t", "Error!z is equal to 2", thisError);
       }

    return array;
}

