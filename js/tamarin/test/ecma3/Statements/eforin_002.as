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
    var SECTION = "forin-002";
    var VERSION = "ECMA_2";
    var TITLE   = "The for...in  statement";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    function MyObject( value ) {
        this.value = value;
	this.valueOf = function(){return this.value;} 
        this.toString = function(){return this.value + '';}
        this.toNumber = function(){return this.value + 0;}
        this.toBoolean = function(){return Boolean( this.value );} 
    }

    var resultArray = new Array( "value", "valueOf", "toString", "toNumber", "toBoolean" );

    //ForIn_1(this);
    //ForIn_2(this);

    ForIn_1(new MyObject(true));
    ForIn_2(new MyObject(true));
    //ForIn_2(new MyObject(new Boolean(true)));

    //ForIn_2(3);

    function ForIn_1( object) {
		var tcValue = item+0;
		var tcValueOf = item+1;
		var tcString = item+2;
		var tcNumber = item+3;
		var tcBoolean = item+4;

        var index = 0;
        with ( object ) {
            for ( property in object ) {
				switch( property ){
					case "value":	
                		array[tcValue] = new TestCase(
                    		SECTION,
                    		"for...in loop in a with loop.  ("+object+")["+property +"] == "+
                        	 property,
                    		true,
                    		property == resultArray[0] );
						item++;
						break;

					case "valueOf":	
                		array[tcValueOf] = new TestCase(
                    		SECTION,
                    		"for...in loop in a with loop.  ("+object+")["+property +"] == "+
                        	 property,
                    		true,
                    		property == resultArray[1] );
						item++;
						break;

					case "toString":	
                		array[tcString] = new TestCase(
                    		SECTION,
                    		"for...in loop in a with loop.  ("+object+")["+property +"] == "+
                        	 property,
                    		true,
                    		property == resultArray[2] );
						item++;
						break;

					case "toNumber":	
                		array[tcNumber] = new TestCase(
                    		SECTION,
                    		"for...in loop in a with loop.  ("+object+")["+property +"] == "+
                        	 property,
                    		true,
                    		property == resultArray[3] );
						item++;
						break;

					case "toBoolean":	
                		array[tcBoolean] = new TestCase(
                    		SECTION,
                    		"for...in loop in a with loop.  ("+object+")["+property +"] == "+
                        	 property,
                    		true,
                    		property == resultArray[4] );
						item++;
						break;


				}
            }
        }
    }

    function ForIn_2(object) {
		var tcValue = item+0;
		var tcValueOf = item+1;
		var tcString = item+2;
		var tcNumber = item+3;
		var tcBoolean = item+4;

        var index = 0
        for ( property in object ) {
            with ( object ) {
				switch( property ){
					case "value":
                		array[tcValue] = new TestCase(
                    		SECTION,
                    		"with loop in a for...in loop.  ("+object+")["+property +"] == "+
                        		property,
                    		true,
                    		property == resultArray[0] );
						item++;
						break;

					case "valueOf":
                		array[tcValueOf] = new TestCase(
                    		SECTION,
                    		"with loop in a for...in loop.  ("+object+")["+property +"] == "+
                        		property,
                    		true,
                    		property == resultArray[1] );
						item++;
						break;

					case "toString":
                		array[tcString] = new TestCase(
                    		SECTION,
                    		"with loop in a for...in loop.  ("+object+")["+property +"] == "+
                        		property,
                    		true,
                    		property == resultArray[2] );
						item++;
						break;

					case "toNumber":
                		array[tcNumber] = new TestCase(
                    		SECTION,
                    		"with loop in a for...in loop.  ("+object+")["+property +"] == "+
                        		property,
                    		true,
                    		property == resultArray[3] );
						item++;
						break;

					case "toBoolean":
                		array[tcBoolean] = new TestCase(
                    		SECTION,
                    		"with loop in a for...in loop.  ("+object+")["+property +"] == "+
                        		property,
                    		true,
                    		property == resultArray[4] );
						item++;
						break;
				}

            }
        }
    }
    return array;
}
