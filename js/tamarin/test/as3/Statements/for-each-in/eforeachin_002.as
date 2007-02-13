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

var SECTION = "Statements";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "for each in";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

 
 // XML Object
 
 var i, s="";
 
 obj1 = {};
 obj1.A = 1;
 
 obj2 = {};
 obj2.A = 2;
 obj3 = {};
 obj3.A = 3;
 
 x1 = {};
 x1.z = [obj1,obj2,obj3];
 
// var xmlDoc = "<L><z><A>1</A></z><z><A>2</A></z><z><A>3</A></z></L>";
// var x1 = new XML(xmlDoc);
 
 for each(var i in x1.z) {
       s += i.A;
 }
 
 AddTestCase( "for-each-in (var) :", true, (s=="123") );
 
 s = 0;
 
 for each ( i in x1.z )
 	s += i.A;
 
 AddTestCase( "for-each-in       :", true, (s==6) );


test();       // leave this alone.  this executes the test cases and
              // displays results.
