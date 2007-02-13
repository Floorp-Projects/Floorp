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

import packOne.*;
import packTwo.*;

var SECTION = " ";
var VERSION = "AS3";
var TITLE   = "import variable from a .abc file [defined in varsDef.as]";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

AddTestCase( "importing 'var1' of packOne", "packOne:var1", var1);
AddTestCase( "importing 'var2' of packOne", "packOne:var2", var2);
AddTestCase( "importing 'var3' of packOne", "packOne:var3", var3);
AddTestCase( "importing 'var4' of packOne", "packOne:var4", var4);

/* Importing variables from the pacakge defined in the second pacakge */
AddTestCase( "importing 'p2var1' of packTwo", "packTwo:var1", p2var1);
AddTestCase( "importing 'p2var2' of packTwo", "packTwo:var2", p2var2);
AddTestCase( "importing 'p2var3' of packTwo", "packTwo:var3", p2var3);
AddTestCase( "importing 'p2var4' of packTwo", "packTwo:var4", p2var4);

test();
