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
    var SECTION = "15.2.4.2";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "Object.prototype.toString()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "(new Object()).toString()",    "[object Object]",  (new Object()).toString() );

	myvar = this;
	myvar.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "myvar = this;  myvar.toString = Object.prototype.toString; myvar.toString()",
                                            GLOBAL,
                                             myvar.toString());


/*	myvar = MyObject;
	myvar.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "myvar = MyObject; myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object Function]",
                                             myvar.toString() );
*/
	myvar = new MyObject( true );
	myvar.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "myvar = new MyObject( true ); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            '[object Object]',
                                            myvar.toString());

	// save
	var origNumProto = Number.prototype.toString;

	Number.prototype.toString=Object.prototype.toString;
	myvar = new Number(0);
    array[item++] = new TestCase( SECTION,  "myvar = new Number(0); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object Number]",
                                             myvar.toString());

    // restore
    Number.prototype.toString = origNumProto

    //save
    var origStrProto = String.prototype.toString;

	String.prototype.toString=Object.prototype.toString;
	myvar = new String('');
	array[item++] = new TestCase( SECTION,  "myvar = new String(''); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object String]",
                                            myvar.toString() );

    // restore
    String.prototype.toString = origStrProto;

	myvar = Math;

	var thisError = "no exception thrown";

// The new Funtion() has been replaced by function() {}
	myvar = function() {};
    
	myvar.toString = Object.prototype.toString;
	array[item++] = new TestCase( SECTION,  "myvar = function() {}; myvar.toString = Object.prototype.toString; myvar.toString()",
                                            true,
                                            myvar.toString()=="[object Function-3]" || myvar.toString()=="[object null]");

/*
	myvar = new Array();
	myvar.toString = Object.prototype.toString;
	array[item++] = new TestCase( SECTION,  "myvar = new Array(); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object Array]",
                                            myvar.toString());

	// save
	var origBoolProto = Boolean.prototype.toString;

	Boolean.prototype.toString=Object.prototype.toString;
	myvar = new Boolean();
   	array[item++] = new TestCase( SECTION,  "myvar = new Boolean(); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object Boolean]",
                                             myvar.toString());

    // restore
    Boolean.prototype.toString = origBoolProto;

	myvar = new Date();
	myvar.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "myvar = new Date(); myvar.toString = Object.prototype.toString; myvar.toString()",
                                            "[object Date]",
                                            myvar.toString());

*/

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object( this ); MYVAR.toString()",
                                            GLOBAL,
                                            (MYVAR = new Object( this ), MYVAR.toString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(); MYVAR.toString()",
                                            "[object Object]",
                                            (MYVAR = new Object(), MYVAR.toString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(void 0); MYVAR.toString()",
                                            "[object Object]",
                                            (MYVAR = new Object(void 0), MYVAR.toString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(null); MYVAR.toString()",
                                            "[object Object]",
                                            (MYVAR = new Object(null), MYVAR.toString() ) );

    return ( array );
}

function MyObject( value ) {
    this.value = function() {return this.value;}
    this.toString = function() {return this.value+'';}
}
