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
    var TITLE   = "Object.prototype.valueOf()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	// save
	var origNumProto = Number.prototype.valueOf;
	var origBoolProto = Boolean.prototype.valueOf;

    var myarray = new Array();
    myarray.valueOf = Object.prototype.valueOf;
    //Boolean.prototype.valueOf=Object.prototype.valueOf;
    //var myboolean = new Boolean();
    var myfunction = function() {};
    myfunction.valueOf = Object.prototype.valueOf;
    var myobject = new Object();
    myobject.valueOf = Object.prototype.valueOf;
   var mymath = Math;
    //mymath.valueOf = Object.prototype.valueOf;
    var mydate = new Date();
    //mydate.valueOf = Object.prototype.valueOf;
   Number.prototype.valueOf=Object.prototype.valueOf;
    var mynumber = new Number();
	//String.prototype.valueOf=Object.prototype.valueOf;
    var mystring = new String();

    array[item++] = new TestCase( SECTION,  "Object.prototype.valueOf.length",      0,      Object.prototype.valueOf.length );

    array[item++] = new TestCase( SECTION,
                                 "myarray = new Array(); myarray.valueOf = Object.prototype.valueOf; myarray.valueOf()",
                                 myarray,
                                 myarray.valueOf() );
    Boolean.prototype.valueOf=Object.prototype.valueOf;
    var myboolean:Boolean = new Boolean();
    var thisError:String="no error";
    try{

       myboolean.valueOf = Object.prototype.valueOf;
       myboolean.valueOf();
    }catch(e:Error){
       thisError=e.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,
                                 "myboolean = new Boolean(); myboolean.valueOf = Object.prototype.valueOf; myboolean.valueOf()",
                                 "ReferenceError: Error #1056",
                                 referenceError(thisError) );
     }
    /*array[item++] = new TestCase( SECTION,
                                 "myboolean = new Boolean(); myboolean.valueOf = Object.prototype.valueOf; myboolean.valueOf()",
                                 myboolean,
                                 myboolean.valueOf() );*/
    array[item++] = new TestCase( SECTION,
                                 "myfunction = function() {}; myfunction.valueOf = Object.prototype.valueOf; myfunction.valueOf()",
                                 myfunction,
                                 myfunction.valueOf() );

    array[item++] = new TestCase( SECTION,
                                 "myobject = new Object(); myobject.valueOf = Object.prototype.valueOf; myobject.valueOf()",
                                 myobject,
                                 myobject.valueOf() );

    try{
       mymath.valueOf = Object.prototype.valueOf;
       mymath.valueOf();
    }catch(e1:Error){
       thisError=e1.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,
        "mymath = Math; mymath.valueOf = Object.prototype.valueOf;mymath.valueOf()",
        "ReferenceError: Error #1056",referenceError(thisError) );
     }
   /* array[item++] = new TestCase( SECTION,
                                 "mymath = Math; mymath.valueOf = Object.prototype.valueOf; mymath.valueOf()",
                                 mymath,
                                 mymath.valueOf() );*/
    try{
       mynumber.valueOf = Object.prototype.valueOf;
       mynumber.valueOf();
    }catch(e2:ReferenceError){
       thisError=e2.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,
        "mynumber = new Number(); mynumber.valueOf = Object.prototype.valueOf; mynumber.valueOf()",
        "ReferenceError: Error #1056",referenceError(thisError) );
     }

    /*array[item++] = new TestCase( SECTION,
                                 "mynumber = new Number(); mynumber.valueOf = Object.prototype.valueOf; mynumber.valueOf()",
                                 mynumber,
                                 mynumber.valueOf() );*/

    try{
       mystring.valueOf = Object.prototype.valueOf;
       mystring.valueOf();
    }catch(e3:Error){
       thisError=e3.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,
        "mystring = new String(); mystring.valueOf = Object.prototype.valueOf; mystring.valueOf()",
        "ReferenceError: Error #1056",referenceError(thisError) );
     }
   /* array[item++] = new TestCase( SECTION,
                                 "mystring = new String(); mystring.valueOf = Object.prototype.valueOf; mystring.valueOf()",
                                 mystring,
                                 mystring.valueOf() );*/

    try{
       mydate.valueOf = Object.prototype.valueOf;
       mydate.valueOf();
    }catch(e4:Error){
       thisError=e4.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,
        "mydate = new Date(); mydate.valueOf = Object.prototype.valueOf; mydate.valueOf()",
        "ReferenceError: Error #1056",referenceError(thisError) );
     }
    /*array[item++] = new TestCase( SECTION,
                                 "mydate = new Date(); mydate.valueOf = Object.prototype.valueOf; mydate.valueOf()",
                                 mydate,
                                 mydate.valueOf());*/

	// restore
	Number.prototype.valueOf = origNumProto;
	Boolean.prototype.valueOf = origBoolProto;

    return ( array );
}
