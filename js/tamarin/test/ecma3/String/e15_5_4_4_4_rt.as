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
    var SECTION = "15.5.4.4-4";
    var VERSION = "ECMA_2";
    startTest();
    var TITLE   = "String.prototype.charAt";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    var thisError="no error";
    var x = null;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eP1:Error){
        thisError=eP1.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","TypeError: Error #1009", typeError(thisError));
    }

    thisError="no error";
    x = null;
    
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP2:Error){
        thisError=eP2.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","TypeError: Error #1009", typeError(thisError));
    }

    thisError="no error";
    x = null;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP3:Error){
        thisError=eP3.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","TypeError: Error #1009", typeError(thisError));
    }
    thisError="no error";
    x = null;
    try{ 
    
    x.__proto.charAt = String.prototype.charAt; 
    x.charAt(3);
    }catch(eP4:Error){
        thisError=eP4.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","TypeError: Error #1009", typeError(thisError));
    }
    /*array[item++] = new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "n",     (x=null; x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "u",     (x=null; x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "l",     (x=null; x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "l",     (x=null; x.__proto__.charAt = String.prototype.charAt, x.charAt(3)) );*/
    
    thisError="no error";
    x = undefined;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eP5:Error){
        thisError=eP5.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","TypeError: Error #1010", typeError(thisError));
    }

    thisError="no error";
    x = undefined;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP6:Error){
        thisError=eP6.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","TypeError: Error #1010", typeError(thisError));
    }

    thisError="no error";
    x = undefined;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP7:Error){
        thisError=eP7.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","TypeError: Error #1010", typeError(thisError));
    }

    thisError="no error";
    x = undefined;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(3);
    }catch(eP8:Error){
        thisError=eP8.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","TypeError: Error #1010", typeError(thisError));
    }

    /*array[item++] = new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "u",     (x=undefined; x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "n",     (x=undefined; x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "d",     (x=undefined; x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "e",     (x=undefined; x.__proto__.charAt = String.prototype.charAt, x.charAt(3)) );*/

    thisError="no error";
    x = false;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eP9:Error){
        thisError=eP9.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = false;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP10:Error){
        thisError=eP10.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = false;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP11:Error){
        thisError=eP11.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = false;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(3);
    }catch(eP12:Error){
        thisError=eP12.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = false;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(4);
    }catch(eP13:Error){
        thisError=eP13.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(4)","ReferenceError: Error #1069", referenceError(thisError));
    }



    /*array[item++] = new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "f",     (x=false, x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "a",     (x=false, x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "l",     (x=false, x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "s",     (x=false, x.__proto__.charAt = String.prototype.charAt, x.charAt(3)) );
    array[item++] = new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(4)",            "e",     (x=false, x.__proto__.charAt = String.prototype.charAt, x.charAt(4)) );*/

    thisError="no error";
    x = true;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eP14:ReferenceError){
        thisError=eP14.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = true;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP15:Error){
        thisError=eP15.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = true;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP16:Error){
        thisError=eP16.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","ReferenceError: Error #1069", referenceError(thisError));
    }
    
    thisError="no error";
    x = true;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(3);
    }catch(eP17:Error){
        thisError=eP17.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = true;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(4);
    }catch(eP18:Error){
        thisError=eP18.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(4)","ReferenceError: Error #1069", referenceError(thisError));
    }

   /* array[item++] = new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "t",     (x=true, x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "r",     (x=true, x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "u",     (x=true, x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "e",     (x=true, x.__proto__.charAt = String.prototype.charAt, x.charAt(3)) );*/

    thisError="no error";
    x = NaN;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eP19:Error){
        thisError=eP19.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = NaN;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP20:Error){
        thisError=eP20.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = NaN;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP21:Error){
        thisError=eP21.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = NaN;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(3);
    }catch(eP22:Error){
        thisError=eP22.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","ReferenceError: Error #1069", referenceError(thisError));
    }
    
    thisError="no error";
    x = NaN;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(4);
    }catch(eP23:Error){
        thisError=eP23.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(4)","ReferenceError: Error #1069", referenceError(thisError));
    }

   /* array[item++] = new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "N",     (x=NaN, x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "a",     (x=NaN, x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "N",     (x=NaN, x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );*/

    thisError="no error";
    x = 123;
    try{ 
    
    x.__proto.charAt = String.prototype.charAt; 
    x.charAt(0);
    }catch(eP24:Error){
        thisError=eP24.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(0)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = 123;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eP25:Error){
        thisError=eP25.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(1)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = 123;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eP26:Error){
        thisError=eP26.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(2)","ReferenceError: Error #1069", referenceError(thisError));
    }

    thisError="no error";
    x = 123;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(3);
    }catch(eP27:Error){
        thisError=eP27.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(3)","ReferenceError: Error #1069", referenceError(thisError));
    }
    
    thisError="no error";
    x = 123;
    try{ 
    
        x.__proto.charAt = String.prototype.charAt; 
        x.charAt(4);
    }catch(eP28:Error){
        thisError=eP28.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x.__proto.charAt = String.prototype.charAt; x.charAt(4)","ReferenceError: Error #1069", referenceError(thisError));
    }

   /* array[item++] = new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "1",     (x=123, x.__proto__.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "2",     (x=123, x.__proto__.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "3",     (x=123, x.__proto__.charAt = String.prototype.charAt, x.charAt(2)) );
*/

    array[item++] = new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     (x=new Array(1,2,3), x.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(1)",            ",",     (x=new Array(1,2,3), x.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(2)",            "2",     (x=new Array(1,2,3), x.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(3)",            ",",     (x=new Array(1,2,3), x.charAt = String.prototype.charAt, x.charAt(3)) );
    array[item++] = new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(4)",            "3",     (x=new Array(1,2,3), x.charAt = String.prototype.charAt, x.charAt(4)) );

    array[item++] = new TestCase( SECTION,  "x = new Array(); x.charAt = String.prototype.charAt; x.charAt(0)",                    "",      (x = new Array(), x.charAt = String.prototype.charAt, x.charAt(0)) );

    thisError="no error";
    x = new Number(123);
    try{ 
    
        x.charAt = String.prototype.charAt; 
        x.charAt(0);
    }catch(eN1:Error){
        thisError=eN1.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)","ReferenceError: Error #1056", referenceError(thisError));
    }

    thisError="no error";
    x = new Number(123);
    try{ 
    
        x.charAt = String.prototype.charAt; 
        x.charAt(1);
    }catch(eN2:Error){
        thisError=eN2.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)","ReferenceError: Error #1056", referenceError(thisError));
    }

    thisError="no error";
    x = new Number(123);
    try{ 
    
        x.charAt = String.prototype.charAt; 
        x.charAt(2);
    }catch(eN3:Error){
        thisError=eN3.toString();
    }finally{
        array[item++] = new TestCase( SECTION,"x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)","ReferenceError: Error #1056", referenceError(thisError));
    }

   /*array[item++] = new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     (x=new Number(123), x.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)",            "2",     (x=new Number(123), x.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)",            "3",     (x=new Number(123), x.charAt = String.prototype.charAt, x.charAt(2)) );*/

    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(0)",            "[",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(3)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(4)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(5)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(6)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(7)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(8)",            "O",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(8)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(9)",            "b",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(9)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(10)",            "j",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(10)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(11)",            "e",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(11)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(12)",            "c",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(12)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(13)",            "t",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(13)) );
    array[item++] = new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(14)",            "]",     (x=new Object(), x.charAt = String.prototype.charAt, x.charAt(14)) );

/* Commenting out due to deferred bug 175096 - could put functionallity to differentiate btwn debug and release player....

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(0)",            "[",    (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(0)) );
    
    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(2)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(3)) );
    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(4)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(5)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(6)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(7)) );


    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(8)",            "F",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(8)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(9)",            "u",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(9)) );
    
    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(10)",            "n",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(10)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(11)",            "c",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(11)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(12)",            "t",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(12)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(13)",            "i",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(13)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(14)",            "o",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(14)) );
    
    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(15)",            "n",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(15)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(16)",            "-",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(16)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(17)",            "1",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(17)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(18)",            "8",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(18)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(19)",            "]",     (x=function() {}, x.toString = Object.prototype.toString, x.charAt = String.prototype.charAt, x.charAt(19)) );


    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(22)","",     (x=function() {}, x.charAt = String.prototype.charAt, x.charAt(22)) );

    array[item++] = new TestCase( SECTION,     "x = function() {}; x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(-1)","",     (x=function() {}, x.charAt = String.prototype.charAt, x.charAt(-1)) );
*/
    return array;
    
}

