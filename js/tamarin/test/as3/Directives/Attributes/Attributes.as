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
package Attrs {

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "Clean AS2";  // Version of JavaScript or ECMA
var TITLE   = "Extend Default Class";       // Provide ECMA section title or a description
//var BUGNUMBER = "";

startTest();                // leave this alone


/*===========================================================================*/

var ATTRS = new Attrs();
// Variables
AddTestCase("var, empty         :", "var, empty         ", ATTRS.emptyVar);
AddTestCase("var, public        :", "var, public        ", ATTRS.pubVar);
var privateVarErr="no error"; try { ATTRS.privVar } catch (e) { privateVarErr=e.toString(); }
AddTestCase("var, private       :", "ReferenceError: Error #1069", privateVarErr.substr(0,27));
//AddTestCase("var, private       :", "var, private       ", ATTRS.privVar);
AddTestCase("var, static        :", "var, static        ", ATTRS.getStatVar());
AddTestCase("var, public static :", "var, public static ", ATTRS.getPubStatVar());
AddTestCase("var, private static:", "var, private static", ATTRS.getPrivStatVar());
AddTestCase("var, static public :", "var, static public ", ATTRS.getStatPubVar());
AddTestCase("var, static private:", "var, static private", ATTRS.getStatPrivVar());

// Functions
AddTestCase("func, empty         :", "func, empty         ", ATTRS.emptyFunc());
AddTestCase("func, public        :", "func, public        ", ATTRS.pubFunc());
var privFunc="no error"; try { ATTRS.privFunc(); } catch (e) { privFunc=e.toString(); }
AddTestCase("func, private       :", "ReferenceError: Error #1069", privateVarErr.substr(0,27));
//AddTestCase("func, private       :", "func, private       ", ATTRS.privFunc());
AddTestCase("func, static        :", "func, static        ", ATTRS.getStatFunc());
AddTestCase("func, public static :", "func, public static ", ATTRS.getPubStatFunc());
AddTestCase("func, private static:", "func, private static", ATTRS.getPrivStatFunc());
AddTestCase("func, static public :", "func, static public ", ATTRS.getStatPubFunc());
AddTestCase("func, static private:", "func, static private", ATTRS.getStatPrivFunc());

// Classes
//var c = new ClassEmpty();
//AddTestCase("class, empty         :", "class, empty         ", c.fn());

//var cpub = new ClassPub();
//AddTestCase("class, public        :", "class, public        ", cpub.fn());

/* The class with 'private' access specifiers are commented */
/*
var cpriv = new ClassPriv();
AddTestCase("class, private       :", "class, private       ", cpriv.fn());
*/

//var cstat = new ClassStat();
//AddTestCase("class, static        :", "class, static        ", cstat.fn());

//var cpubstat = new ClassPubStat();
//AddTestCase("class, public static :", "class, public static ", cpubsat.fn());

/*
var cprivstat = new ClassPrivStat();
AddTestCase("class, private static:", "class, private static", cpriv.fn());
*/

//var cstatpub = new ClassStatPub();
//AddTestCase("class, static public :", "class, static public ", cstatpub.fn());

/*
var cstatpriv = new ClassStatPriv();
AddTestCase("class, static private:", "class, static private", cstatpriv.fn());
*/

// Interfaces
//var i = new IfEmpty_();
//AddTestCase("interface, empty         :", "interface, empty         ", i.fn());

//var ipub = new IfPub_();
//AddTestCase("interface, public        :", "interface, public        ", ipub.fn());

/*
var ipriv = new IfPriv_();
AddTestCase("interface, private       :", "interface, private       ", ipriv.fn());
*/

//var istat = new IfStat_();
//AddTestCase("interface, static        :", "interface, static        ", istat.fn());

//var ipubstat = new IfPubStat_();
//AddTestCase("interface, public static :", "interface, public static ", ipubstat.fn());

/*
var iprivstat = new IfPrivStat_();
AddTestCase("interface, private static:", "interface, private static", iprivstat.fn());
*/

//import Directives.Attributes.IfStatPub_;
//var istatpub = new IfStatPub_();
//AddTestCase("interface, static public :", "interface, static public ", istatpub.fn());

/*
var istatpriv = new IfStatPriv_();
AddTestCase("interface, static private:", "interface, static private", istatpriv.fn());
*/

/*===========================================================================*/

test();       // leave this alone.  this executes the test cases and
              // displays results.
}