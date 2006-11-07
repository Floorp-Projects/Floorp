/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
    var SECTION = "RegExp/properties-001.js";
    var VERSION = "ECMA_2";
    var TITLE   = "Properties of RegExp Instances";
    var BUGNUMBER ="http://scopus/bugsplat/show_bug.cgi?id=346000";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

    AddRegExpCases( new RegExp, "",   false, false, false, 0 );
    AddRegExpCases( /.*/,       ".*", false, false, false, 0 );
    AddRegExpCases( /[\d]{5}/g, "[\\d]{5}", true, false, false, 0 );
    AddRegExpCases( /[\S]?$/i,  "[\\S]?$", false, true, false, 0 );
    AddRegExpCases( /^([a-z]*)[^\w\s\f\n\r]+/m,  "^([a-z]*)[^\\w\\s\\f\\n\\r]+", false, false, true, 0 );
    AddRegExpCases( /[\D]{1,5}[\ -][\d]/gi,      "[\\D]{1,5}[\\ -][\\d]", true, true, false, 0 );
    AddRegExpCases( /[a-zA-Z0-9]*/gm, "[a-zA-Z0-9]*", true, false, true, 0 );
    AddRegExpCases( /x|y|z/gim, "x|y|z", true, true, true, 0 );

    AddRegExpCases( /\u0051/im, "\u0051", false, true, true, 0 );
    AddRegExpCases( /\x45/gm, "\\x45", true, false, true, 0 );
    AddRegExpCases( /\097/gi, "\\097", true, true, false, 0 );

	function AddRegExpCases( re, s, g, i, m, l ) {

	    array[item++] = new TestCase(SECTION,
	    			 re + ".test == RegExp.prototype.test",
	                 true,
	                 re.test == RegExp.prototype.test );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".toString == RegExp.prototype.toString",
	                 true,
	                 re.toString == RegExp.prototype.toString );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".constructor == RegExp.prototype.constructor",
	                 true,
	                 re.constructor == RegExp.prototype.constructor );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".compile == RegExp.prototype.compile",
	                 true,
	                 re.compile == RegExp.prototype.compile );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".exec == RegExp.prototype.exec",
	                 true,
	                 re.exec == RegExp.prototype.exec );

	    // properties

	    array[item++] = new TestCase(SECTION,
	    			 re + ".source",
	                 s,
	                 re.source );
	    try{
	    	re.source = "k";
	    }catch(e:ReferenceError){
	        thisError = e.toString();
	    }finally{
	        array[item++] = new TestCase(SECTION,
	        		 re + ".source='k'",
	                 "ReferenceError: Error #1074",
	                 referenceError(thisError) );
	    }



	    array[item++] = new TestCase(SECTION,
	    			 "delete"+re + ".source",
	                 false,
	                delete re.source);



	    array[item++] = new TestCase(SECTION,
	    			 re + ".toString()",
	                 "/" + s +"/" + (g?"g":"") + (i?"i":"") +(m?"m":""),
	                 re.toString() );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".global",
	                 g,
	                 re.global );

	     try{
	        re.global = true;
	    }catch(e:ReferenceError){
	         thisError = e.toString();
	    }finally{
	         array[item++] = new TestCase(SECTION,
	         		 re + ".global=true",
	                 "ReferenceError: Error #1074",
	                 referenceError(thisError) );
	    }



	    array[item++] = new TestCase(SECTION,
	    			 "delete"+re + ".global",
	                 false,
	                delete re.global);

	    array[item++] = new TestCase(SECTION,
	    			 re + ".ignoreCase",
	                 i,
	                 re.ignoreCase );

	    try{
	        re.ignoreCase = true;
	    }catch(e:ReferenceError){
	         thisError = e.toString();
	    }finally{
	         array[item++] = new TestCase(SECTION,
	         		 re + ".ignoreCase=true",
	                 "ReferenceError: Error #1074",
	                 referenceError(thisError) );
	    }



	    array[item++] = new TestCase(SECTION,
	    			 "delete"+re + ".ignoreCase",
	                 false,
	                delete re.ignoreCase);

	    array[item++] = new TestCase(SECTION,
	    			 re + ".multiline",
	                 m,
	                 re.multiline);

	    try{
	        re.multiline = true;
	    }catch(e:ReferenceError){
	         thisError = e.toString();
	    }finally{
	         array[item++] = new TestCase(SECTION,
	         		 re + ".multiline=true",
	                 "ReferenceError: Error #1074",
	                 referenceError(thisError) );
	    }



	    array[item++] = new TestCase(SECTION,
	    			 "delete"+re + ".multiline",
	                 false,
	                delete re.multiline);

	    array[item++] = new TestCase(SECTION,
	    			 re + ".lastIndex",
	                 l,
	                 re.lastIndex  );

	    array[item++] = new TestCase(SECTION,
	    			 "delete"+re + ".lastIndex",
	                 false,
	                delete re.lastIndex);
	}

	return array;
}
