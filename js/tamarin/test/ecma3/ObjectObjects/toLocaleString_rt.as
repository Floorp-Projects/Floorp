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
    var SECTION = "15.2.4.3";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "Object.prototype.toLocaleString()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "(new Object()).toLocaleString()",    "[object Object]",  (new Object()).toLocaleString() );

    array[item++] = new TestCase( SECTION,  "myvar = this;  myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            GLOBAL,
                                            (myvar = this,  myvar.toLocaleString = Object.prototype.toLocaleString, myvar.toLocaleString()) );

	// work around for bug 175820
	var expRes = "[object Function-4]";
	try{
		if(Capabilities.isDebugger == false)
			expRes = "[object null]";
	} catch( sd ) {
		// do nothing
	}
    array[item++] = new TestCase( SECTION,  "myvar = MyObject; myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            expRes,
                                            (myvar = MyObject, myvar.toLocaleString = Object.prototype.toLocaleString, myvar.toLocaleString()) );

    array[item++] = new TestCase( SECTION,  "myvar = new MyObject( true ); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            '[object Object]',
                                            (myvar = new MyObject( true ), myvar.toLocaleString = Object.prototype.toLocaleString, myvar.toLocaleString()) );

	Number.prototype.toLocaleString;
	//Object.prototpye.toLocaleString;
    array[item++] = new TestCase( SECTION,  "myvar = new Number(0); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "0",
                                            (myvar = new Number(0), myvar.toLocaleString()) );

/*    array[item++] = new TestCase( SECTION,  "myvar = new String(''); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object String]",
                                            eval("myvar = new String(''); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "myvar = Math; myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object Math]",
                                            eval("myvar = Math; myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "myvar = function() {}; myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object Function]",
                                            eval("myvar = function() {}; myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "myvar = new Array(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object Array]",
                                            eval("myvar = new Array(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "myvar = new Boolean(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object Boolean]",
                                            eval("myvar = new Boolean(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "myvar = new Date(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()",
                                            "[object Date]",
                                            eval("myvar = new Date(); myvar.toLocaleString = Object.prototype.toLocaleString; myvar.toLocaleString()") );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object( this ); MYVAR.toLocaleString()",
                                            GLOBAL,
                                            (MYVAR = new Object( this ), MYVAR.toLocaleString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(); MYVAR.toLocaleString()",
                                            "[object Object]",
                                            (MYVAR = new Object(), MYVAR.toLocaleString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(void 0); MYVAR.toLocaleString()",
                                            "[object Object]",
                                            (MYVAR = new Object(void 0), MYVAR.toLocaleString() ) );

    array[item++] = new TestCase( SECTION,  "var MYVAR = new Object(null); MYVAR.toLocaleString()",
                                            "[object Object]",
                                            (MYVAR = new Object(null), MYVAR.toLocaleString() ) );
*/
    return ( array );
}

function MyObject( value ) {
    this.value = function() { return this.value; }
    this.toLocaleString = function() { return this.value+''; }

    this.value = function() {return this.value;}
    this.toLocaleString = function() {return this.value+'';}
}
