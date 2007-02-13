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
    var SECTION = "15.5.4.5-6";
    var VERSION = "ECMA_2";
    startTest();
    var TITLE   = "String.prototype.charCodeAt";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    var thisError="no error";
    var obj = true;
    var s = ''
    
    var origBooleanCharCodeAt = Boolean.prototype.charCodeAt;
    Boolean.prototype.charCodeAt= String.prototype.charCodeAt;
    try{ 
    
        obj.__proto__.charAt = String.prototype.charAt;
        for ( var i = 0; i < 4; i++ ) 
            s+= String.fromCharCode( obj.charCodeAt(i) ); 
    }catch(eP0:ReferenceError){
        thisError=eP0.toString();
    }
    array[item++] = new TestCase( SECTION,"var obj = true; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s","ReferenceError: Error #1069", referenceError(thisError));
     

    /*var obj = true; 
    obj.__proto__.charCodeAt = String.prototype.charCodeAt;
    var s = '';
    for ( var i = 0; i < 4; i++ ) 
        s+= String.fromCharCode( obj.charCodeAt(i) );

    array[item++] = new TestCase( SECTION,
                                  "var obj = true; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s",
                                  "true",
                                  s);*/

    thisError="no error";
    var obj = 1234;
    var s = '';
    try{ 
    
        obj.__proto__.charAt = String.prototype.charAt;
        for ( var i = 0; i < 4; i++ ) 
            s+= String.fromCharCode( obj.charCodeAt(i) ); 
    }catch(eP1:Error){
        thisError=eP1.toString();
    }
    array[item++] = new TestCase( SECTION,"var obj = 1234; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s", "ReferenceError: Error #1069",referenceError(thisError));
     

    /*obj = 1234;
    obj.__proto__.charCodeAt = String.prototype.charCodeAt;
    s = '';
    for ( var i = 0; i < 4; i++ ) 
        s+= String.fromCharCode( obj.charCodeAt(i) );

    array[item++] = new TestCase( SECTION,
                                  "var obj = 1234; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s",
                                  "1234",
                                   s);*/
    thisError="no error";
    var obj = 'hello';
    var s = '';
    try{ 
    
        obj.__proto__.charAt = String.prototype.charAt;
        for ( var i = 0; i < 4; i++ ) 
            s+= String.fromCharCode( obj.charCodeAt(i) ); 
    }catch(eP2:Error){
        thisError=eP2.toString();
    }
    array[item++] = new TestCase( SECTION,"var obj = 1234; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s", "ReferenceError: Error #1069",referenceError(thisError));
     

   /* obj = 'hello';
    obj.__proto__.charCodeAt = String.prototype.charCodeAt;
    s = '';
    for ( var i = 0; i < 5; i++ ) 
        s+= String.fromCharCode( obj.charCodeAt(i) );

    array[item++] = new TestCase( SECTION,
                                  "var obj = 'hello'; obj.__proto__.charCodeAt = String.prototype.charCodeAt; var s = ''; for ( var i = 0; i < 5; i++ ) s+= String.fromCharCode( obj.charCodeAt(i) ); s",
                                  "hello",
                                  s );*/

    var myvar = new String(true);
    var s = '';
    for ( var i = 0; i < 4; i++ ) 
        s+= String.fromCharCode( myvar.charCodeAt(i)) 
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String(true); var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( myvar.charCodeAt(i) ); s",
                                  "true",
                                  s);

    var myvar = new String(1234);
    var s = '';
    for ( var i = 0; i < 4; i++ ) 
        s+= String.fromCharCode( myvar.charCodeAt(i)) 
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String(1234); var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( myvar.charCodeAt(i) ); s",
                                  "1234",
                                  s);

    var myvar = new String('hello');
    var s = '';
    for ( var i = 0; i < myvar.length; i++ ) 
        s+= String.fromCharCode( myvar.charCodeAt(i)) 
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String('hello'); var s = ''; for ( var i = 0; i < 4; i++ ) s+= String.fromCharCode( myvar.charCodeAt(i) ); s",
                                  "hello",
                                  s);

    var myvar = new String('hello');
    var s = '';
    s = myvar.charCodeAt(-1);
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String('hello'); var s = myvar.charCodeAt(-1)",NaN,s);

    var myvar = new String(1234);
    var s = '';
    s = myvar.charCodeAt(0);
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String(1234); var s = myvar.charCodeAt(0)",49,s);

    var myvar = new String(1234);
    var s = '';
    s = String.fromCharCode(myvar.charCodeAt());
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String(1234); var s = String.fromCharCode(myvar.charCodeAt())","1",s);

    var myvar = new String(1234);
    var s = '';
    s = myvar.charCodeAt(5);
    
    array[item++] = new TestCase( SECTION,
                                  "var myvar = new String(1234); var s = myvar.charCodeAt(5)",NaN,s);

    var myobj = new Object();
    myobj.length = 5
    myobj.charCodeAt = String.prototype.charCodeAt;
    myobj[0]='h';
    myobj[1]='e';
    myobj[2]='l';
    myobj[3]='l';
    myobj[4]='o';
    array[item++] = new TestCase( SECTION,
                                  "var myobj = new Object();myobj.charCodeAt = String.prototype.charCodeAt;  myobj.charCodeAt(4)",101,myobj.charCodeAt(4));


    Boolean.prototype.charCodeAt= origBooleanCharCodeAt;

    return (array );
}
