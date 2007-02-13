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
package Prototyping {



import PublicClassPrototype.*;

var SECTION = "Class Prototype";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Testing prototype for public classes";  // Provide ECMA section title or a description
//var BUGNUMBER = "";

startTest();                // leave this alone


var publicObj = new PublicClass();


PublicClass.prototype.array = new Array('a', 'b', 'c');
AddTestCase( "Try overriding default property through a public class' prototype object", "1,2,3", publicObj.accessDefaultProperty().toString() );

PublicClass.prototype.intNumber = 500;
AddTestCase( "Try overriding internal property through a public class' prototype object", "100", publicObj.intNumber.toString() );

PublicClass.prototype.protInt = 0;	// Note: this override works because the protected property is not visible!
AddTestCase( "Try overriding protected property through a public class' prototype object", "0", publicObj.protInt.toString() );

PublicClass.prototype.pubUint = 0;
AddTestCase( "Try overriding public property through a public class' prototype object", "1", publicObj.pubUint.toString() );

PublicClass.prototype.privVar = false;
AddTestCase( "Try overriding private property through a public class' prototype object", "true", publicObj.accPrivProp().toString() );

PublicClass.prototype.pubStat = 200;
AddTestCase( "Try overriding public static property through a public class' prototype object", "100", PublicClass.pubStat.toString() );

PublicClass.prototype.nsProp = "fakeNS";
AddTestCase( "Try overriding namespace property through a public class' prototype object", "nsProp", publicObj.accNS().toString() );

PublicClass.prototype.defaultMethod = false;
AddTestCase( "Try overriding default methodsthrough a public class' prototype object", true, publicObj.defaultMethod() );

PublicClass.prototype.internalMethod = -1;
AddTestCase( "Try overriding internal method through a public class' prototype object", 1, publicObj.internalMethod() );

//PublicClass.prototype.protectedMethod = -1;
//AddTestCase( "Try overriding protected method through a public class' prototype object", 1, publicObj.protectedMethod() );

PublicClass.prototype.publicMethod = false;
AddTestCase( "Try overriding public method through a public class' prototype object", true, publicObj.publicMethod() );

PublicClass.prototype.privateMethod = false;
AddTestCase( "Try overriding private method through a public class' prototype object", true, publicObj.accPrivMethod() );

PublicClass.prototype.nsMethod = -1;
AddTestCase( "Try overriding namespace method through a public class' prototype object", 1, publicObj.accNSMethod() );

PublicClass.prototype.publicFinalMethod = -1;
AddTestCase( "Try overriding public final method through a public class' prototype object", 1, publicObj.publicFinalMethod() );

PublicClass.prototype.publicStaticMethod = -1;
AddTestCase( "Try overriding public static method through a public class' prototype object", 42, PublicClass.publicStaticMethod() );


PublicClass.prototype.newArray = new Array('a', 'b', 'c');
AddTestCase( "Try adding new property through a public class' prototype object", "a,b,c", publicObj.newArray.toString() );

PublicClass.prototype.testFunction = function () {return true};
AddTestCase("Try adding new method through a public class' prototype object", true, publicObj.testFunction());

var equivalent:Boolean = (PublicClass.prototype.constructor == PublicClass);
AddTestCase("Verify prototype constructor is equivalent to class object", true, equivalent);


var thisError10 = "no error thrown";
var temp:Object = new Object();
try{
	PublicClass.prototype = temp;
} catch (e:ReferenceError) {
	thisError10 = e.toString();
} finally {
	AddTestCase( "Try to write to PublicClass' prototype object", "ReferenceError: Error #1074",
				referenceError( thisError10 ) );
}

test();       // leave this alone.  this executes the test cases and
              // displays results.

}
