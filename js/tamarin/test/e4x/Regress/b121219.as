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
START("QName in nested functions");

var bug = 121219;
var actual = "";
var expect = "no error";

function doFrame() {
    function nestedFunc(a, b, c) {
    }
    
    x1 =
    <alpha>
    <bravo attr1="value1" ns:attr1="value3" xmlns:ns="http://someuri">
    <charlie attr1="value2" ns:attr1="value4"/>
    </bravo>
    </alpha>

    var n = new Namespace("http://someuri");

    q = new QName(n, "attr1");
    nestedFunc(7, "value3value4", x1..@[q]);

    var xml1 = "<c><color c='1'>pink</color><color c='2'>purple</color><color c='3'>orange</color></c>";
    var placeHolder = "c";

    nestedFunc("x1.node1[i].@attr", "1",( x2 = new XML(xml1), x2.color[0].@c.toString()));
}

try {
	doFrame();
	actual = "no error";
} catch(e1) {
	actual = "error thrown: " + e1.toString();
}
	


AddTestCase("QName in nested function", expect, actual);

END();
