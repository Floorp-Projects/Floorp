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

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Interface Definition";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

//-----------------------------------------------------------------------------

import QualUnqualAccess.*;

var eg = new AccessTest();
AddTestCase("single implements, default, interface, class", "a.T::f(),a.T::f(),a.T::f()", eg.doAccessA());
AddTestCase("single implements (extends and add), default, interface, base, class", "b.T::f(),b.T::f(),b.T::f(),b.T::f()", eg.doAccessBF());
AddTestCase("single implements (extends and add), default, interface, class", "b.U::g(),b.U::g(),b.U::g()", eg.doAccessBG());
AddTestCase("double implements, default, interface, class", "c.T::f(),c.T::f(),c.T::f()", eg.doAccessCF());

AddTestCase("double implements, default, interface, class", "c.V::h(),c.V::h(),c.V::h()", eg.doAccessCH());

AddTestCase("double implements (extends and add), default, interface, base, class", "d.T::f(),d.T::f(),d.T::f(),d.T::f()", eg.doAccessDF());

AddTestCase("double implements (extends and add), default, interface, class", "d.U::g(),d.U::g(),d.U::g()", eg.doAccessDG());

AddTestCase("double implements, default, interface, class", "d.V::h(),d.V::h(),d.V::h()", eg.doAccessDH());

AddTestCase("extends (implements (extends and add)) and add, default, interface, base, class",
				"b.T::f(),b.T::f(),b.T::f(),b.T::f()", eg.doAccessEF());

AddTestCase("extends (implements (extends and add)) and add, default, interface, class",
				"b.U::g(),b.U::g(),b.U::g()", eg.doAccessEG());


AddTestCase("extends and add, default, interface, class",
				"e.E::h()", eg.doAccessEH());

AddTestCase("extends (implements (extends and add)) and add, default, interface, base, class",
				"b.T::f(),b.T::f(),b.T::f(),b.T::f()", eg.doAccessFF());

AddTestCase("extends (implements (extends and add)) and add, default, interface, class",
				"b.U::g(),b.U::g(),b.U::g()", eg.doAccessFG());
/*
AddTestCase("extends and add, default, interface, class",
				"f.V::h(),f.V::h(),f.V::h()", eg.doAccessFH());
*/
test();       // leave this alone.  this executes the test cases and
              // displays results.
