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
    use namespace AS3;
    var SECTION = "15.5.4.6-2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.protoype.indexOf";
    var BUGNUMBER="105721";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();


// the following test regresses http://scopus/bugsplat/show_bug.cgi?id=105721

function f() {
    return this;
}
function g() {
    var h = f;
    return h();
}

function MyObject (v) {
    this.value      = v;
    this.toString   = function () { return this.value +""; };
    this.indexOf     = String.prototype.indexOf;
}

function getTestCases() {
    var array = new Array();
    var item = 0;

    // regress http://scopus/bugsplat/show_bug.cgi?id=105721

    array[item++] = new TestCase( SECTION, "function f() { return this; }; function g() { var h = f; return h(); }; g().toString()",    GLOBAL,  g().toString() );
    
    array[item++] = new TestCase( SECTION, "String.prototype.indexOf.length",                                               2,     String.prototype.indexOf.length );
    //length property is read-only
    //array[item++] = new TestCase( SECTION, "String.prototype.indexOf.length = null; String.prototype.indexOf.length",       2,     (String.prototype.indexOf.length=null,String.prototype.indexOf.length) );
    array[item++] = new TestCase( SECTION, "delete String.prototype.indexOf.length",                                        false,  delete String.prototype.indexOf.length );
    array[item++] = new TestCase( SECTION, "delete String.prototype.indexOf.length; String.prototype.indexOf.length",       2,      (delete String.prototype.indexOf.length, String.prototype.indexOf.length) );

    array[item++] = new TestCase( SECTION, "var s = new String(), s.indexOf()",     -1,     (s = new String(), s.indexOf() ) );

    // some Unicode tests.

    // generate a test string.

    var TEST_STRING = "";

    for ( var u = 0x00A1; u <= 0x00FF; u++ ) {
        TEST_STRING += String.fromCharCode( u );
    }

    for ( var u = 0x00A1, i = 0; u <= 0x00FF; u++, i++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "TEST_STRING.indexOf( " + String.fromCharCode(u) + " )",
                                        i,
                                        TEST_STRING.indexOf( String.fromCharCode(u) ) );
    }
    for ( var u = 0x00A1, i = 0; u <= 0x00FF; u++, i++ ) {
        array[item++] = new TestCase(   SECTION,
                                        "TEST_STRING.indexOf( " + String.fromCharCode(u) + ", void 0 )",
                                        i,
                                        TEST_STRING.indexOf( String.fromCharCode(u), void 0 ) );
    }



    var foo = new MyObject('hello');

    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('h')", 0, foo.indexOf("h")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('e')", 1, foo.indexOf("e")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('l')", 2, foo.indexOf("l")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('l')", 2, foo.indexOf("l")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('o')", 4, foo.indexOf("o")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf('X')", -1,  foo.indexOf("X")  );
    array[item++] = new TestCase( SECTION, "var foo = new MyObject('hello');foo.indexOf(5) ", -1,  foo.indexOf(5)  );

    var boo = new MyObject(true);

    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('t')", 0, boo.indexOf("t")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('r')", 1, boo.indexOf("r")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('u')", 2, boo.indexOf("u")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('e')", 3, boo.indexOf("e")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('true')", 0, boo.indexOf("true")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('rue')", 1, boo.indexOf("rue")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('ue')", 2, boo.indexOf("ue")  );
    array[item++] = new TestCase( SECTION, "var boo = new MyObject(true);boo.indexOf('oy')", -1, boo.indexOf("oy")  );


    var noo = new MyObject( Math.PI );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('3') ", 0, noo.indexOf('3')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('.') ", 1, noo.indexOf('.')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('1') ", 2, noo.indexOf('1')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('4') ", 3, noo.indexOf('4')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('1') ", 2, noo.indexOf('1')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('5') ", 5, noo.indexOf('5')  );
    array[item++] = new TestCase( SECTION, "var noo = new MyObject(Math.PI); noo.indexOf('9') ", 6, noo.indexOf('9')  );

    array[item++] = new TestCase( SECTION,
                                  "var arr = new Array('new','zoo','revue'); arr.indexOf = String.prototype.indexOf; arr.indexOf('new')",
                                  0,
                                  (arr = new Array('new','zoo','revue'), arr.indexOf('new') ) );

    array[item++] = new TestCase( SECTION,
                                  "var arr = new Array('new','zoo','revue'); arr.indexOf = String.prototype.indexOf; arr.indexOf(',zoo,')",
                                  1,
                                  (arr = new Array('new','zoo','revue'), arr.indexOf('zoo') ) );
    


    array[item++] = new TestCase( SECTION,
                                  "var obj = new Object(); obj.indexOf = String.prototype.indexOf; obj.indexOf('[object Object]')",
                                  0,
                                  (obj = new Object(), obj.indexOf = String.prototype.indexOf, obj.indexOf('[object Object]') ) );

    array[item++] = new TestCase( SECTION,
                                  "var obj = new Object(); obj.indexOf = String.prototype.indexOf; obj.indexOf('bject')",
                                  2,
                                  (obj = new Object(), obj.indexOf = String.prototype.indexOf, obj.indexOf('bject') ) );

    array[item++] = new TestCase( SECTION,
                                  "var f = new Object( String.prototype.indexOf ); f('"+GLOBAL+"')",
                                  0,
                                  (f = new Object( String.prototype.indexOf ), f(String(GLOBAL))) );

	// work around for bug 175820
	var expRes1 = "[object Function-97]";
	if( !Capabilities.isDebugger )
		expRes1 = "[object null]";

    array[item++]= new TestCase(SECTION,"Assigning Object.prototype.toString to f.toString",expRes1, (f.toString=Object.prototype.toString, f.toString()));
        
	// work around for bug 175820
	var expRes2 = 0;
	if( !Capabilities.isDebugger )
		expRes2 = -1;

    array[item++] = new TestCase( SECTION,
                                  "var f = function() {}; f.toString = Object.prototype.toString; f.indexOf = String.prototype.indexOf; f.indexOf('[object Function-0]')",
                                   expRes2,
                                   (f = function() {}, f.toString = Object.prototype.toString, f.indexOf = String.prototype.indexOf, f.indexOf('[object Function-0]') ) );

    try{
        var b = new Boolean(); 
        b.indexOf = String.prototype.indexOf; 
        b.indexOf('true');
    }
    catch(e2){
        thisError=e2.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('true')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        var b = new Boolean(); 
        b.indexOf = String.prototype.indexOf; 
        b.indexOf('false',1);
    }
    catch(e3){
        thisError=e3.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('true')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        var b = new Boolean(); 
        b.indexOf = String.prototype.indexOf; 
        b.indexOf('false',0);
    }
    catch(e4){
        thisError=e4.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('true')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        n = new Number(1e21); 
        n.indexOf = String.prototype.indexOf; 
        n.indexOf('e'); 
    }
    catch(e5){
        thisError=e5.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var n = new Number(1e21); n.indexOf = String.prototype.indexOf; n.indexOf('e')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        n = new Number(-Infinity); 
        n.indexOf = String.prototype.indexOf; 
        n.indexOf('-'); 
    }
    catch(e6){
        thisError=e6.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var n = new Number(-Infinity); n.indexOf = String.prototype.indexOf; n.indexOf('-')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        var n = new Number(0xFF); 
        n.indexOf = String.prototype.indexOf; 
        n.indexOf('5');
    }
    catch(e7){
        thisError=e7.toString();
    }
    finally{
        array[item++] = new TestCase( SECTION,
                                  "var n = new Number(0xFF); n.indexOf = String.prototype.indexOf; n.indexOf('5')","ReferenceError: Error #1056",referenceError(thisError));
    }
    try{
        var m = Math; 
        m.indexOf = String.prototype.indexOf; m.indexOf( 'Math' );
    }catch(e8){
        thisError=e8.toString();
    }finally{
        array[item++] = new TestCase( SECTION,
                                  "var m = Math; m.indexOf = String.prototype.indexOf; m.indexOf( 'Math' )","ReferenceError: Error #1056",referenceError(thisError));
             
    }


 /*array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('true')",
                                  -1,
                                  (b = new Boolean(), b.indexOf = String.prototype.indexOf, b.indexOf('true') ) );

    array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('false', 1)",
                                  -1,
                                  (b = new Boolean(), b.indexOf = String.prototype.indexOf, b.indexOf('false', 1) ) );

    array[item++] = new TestCase( SECTION,
                                  "var b = new Boolean(); b.indexOf = String.prototype.indexOf; b.indexOf('false', 0)",
                                  0,
                                  (b = new Boolean(), b.indexOf = String.prototype.indexOf, b.indexOf('false', 0) ) );

  array[item++] = new TestCase( SECTION,
                                  "var n = new Number(1e21); n.indexOf = String.prototype.indexOf; n.indexOf('e')",
                                  1,
                                  (n = new Number(1e21), n.indexOf = String.prototype.indexOf, n.indexOf('e') ) );

    array[item++] = new TestCase( SECTION,
                                  "var n = new Number(-Infinity); n.indexOf = String.prototype.indexOf; n.indexOf('-')",
                                  0,
                                  (n = new Number(-Infinity), n.indexOf = String.prototype.indexOf, n.indexOf('-') ) );

    array[item++] = new TestCase( SECTION,
                                  "var n = new Number(0xFF); n.indexOf = String.prototype.indexOf; n.indexOf('5')",
                                  1,
                                  (n = new Number(0xFF), n.indexOf = String.prototype.indexOf, n.indexOf('5') ) );

    array[item++] = new TestCase( SECTION,
                                  "var m = Math; m.indexOf = String.prototype.indexOf; m.indexOf( 'Math' )",
                                  8,
                                  (m = Math, m.indexOf = String.prototype.indexOf, m.indexOf( 'Math' ) ) );*/

    // new Date(0) has '31' or '01' at index 8 depending on whether tester is (GMT-) or (GMT+), respectively
    array[item++] = new TestCase( SECTION,
                                  "var d = new Date(0); d.indexOf = String.prototype.indexOf; d.getTimezoneOffset()>0 ? d.indexOf('31') : d.indexOf('01')",
                                  8,
                                  (d = new Date(0), d.indexOf = String.prototype.indexOf, d.getTimezoneOffset()>0 ? d.indexOf('31') : d.indexOf('01') ) );


    return array;
}
