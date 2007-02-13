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
    var SECTION = "try-006";
    var VERSION = "ECMA_2";
    var TITLE   = "The try statement";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);


    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
    
    function throwException() {
        throw EXCEPTION_STRING +": " + this.valueOf();
    }
    var EXCEPTION_STRING = "Exception thrown:";

    function noException() {
        return this.valueOf();
    }

    TryWith( new TryObject( "hello", throwException, true ));
    TryWith( new TryObject( "hola",  noException, false ));



    function TryObject( value, fun, exception ) {
        this.value = value;
        this.exception = exception;

       // this.valueOf = new Function ( "return this.value" );
        this.valueOf = function(){return this.value;}
        this.check = fun;
    }

    function TryWith( object ) {
        try {
            with ( object ) {
                result = check();
            }
        } catch ( e ) {
            result = e;
        }
        array[item++] = new TestCase(
            SECTION,
            "TryWith( " + object.value +" )",
            (object.exception ? EXCEPTION_STRING +": " + object.valueOf() : object.valueOf()),
            result );
        with (Object)                   //Added this as bug #139065 is fixed
           {
             try{
                 undeclR = undecl;
            }catch (e1){
                 result = e1.toString();
             }
           }
           
        array[item++] = new TestCase(
            SECTION,
            "TryWith( " + object.value +" )",
            "ReferenceError: Error #1065",
            referenceError(result) );

        
    }
    
    return array;
}
