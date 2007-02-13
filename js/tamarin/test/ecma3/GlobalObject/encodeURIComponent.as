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

    var SECTION = "15.1.3.4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "encodeURIComponent";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    str1 = new String("h");

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('')", "",  encodeURIComponent("") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent(str1)", "h",  encodeURIComponent(str1) );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('Hello{[World]}')", "Hello%7B%5BWorld%5D%7D",  encodeURIComponent("Hello{[World]}") );
    array[item++] = new TestCase( SECTION,  "encodeURIComponent('Macromedia - Flash')", "Macromedia%20-%20Flash",  encodeURIComponent("Macromedia - Flash") );
    array[item++] = new TestCase( SECTION,  "encodeURIComponent('2 * 4 + [8 + 5 ] - 3')", "2%20*%204%20%2B%20%5B8%20%2B%205%20%5D%20-%203",  encodeURIComponent("2 * 4 + [8 + 5 ] - 3") );
    array[item++] = new TestCase( SECTION,  "encodeURIComponent('Flash(Macromedia)')", "Flash(Macromedia)",  encodeURIComponent("Flash(Macromedia)") );
    array[item++] = new TestCase( SECTION,  "encodeURIComponent('BugID #17485')", "BugID%20%2317485",  encodeURIComponent("BugID #17485") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flasha player')", "http%3A%2F%2Fwww.macromedia.com%2Fflasha%20player",  encodeURIComponent("http://www.macromedia.com/flasha player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flashA player')", "http%3A%2F%2Fwww.macromedia.com%2FflashA%20player",  encodeURIComponent("http://www.macromedia.com/flashA player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash_ player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash_%20player",  encodeURIComponent("http://www.macromedia.com/flash_ player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash- player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash-%20player",  encodeURIComponent("http://www.macromedia.com/flash- player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash. player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash.%20player",  encodeURIComponent("http://www.macromedia.com/flash. player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash! player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash!%20player",  encodeURIComponent("http://www.macromedia.com/flash! player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash~ player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash~%20player",  encodeURIComponent("http://www.macromedia.com/flash~ player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash* player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash*%20player",  encodeURIComponent("http://www.macromedia.com/flash* player") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/'flash player'')", "http%3A%2F%2Fwww.macromedia.com%2F'flash%20player'",  encodeURIComponent("http://www.macromedia.com/'flash player'") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/(flash player)')", "http%3A%2F%2Fwww.macromedia.com%2F(flash%20player)",  encodeURIComponent("http://www.macromedia.com/(flash player)") );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http%3A%2F%2Fwww.macromedia.com/flash; player')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%3B%20player",  encodeURIComponent("http://www.macromedia.com/flash; player")+"" );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash player?')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3F",  encodeURIComponent("http://www.macromedia.com/flash player?")+"" );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash player@')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%40",  encodeURIComponent("http://www.macromedia.com/flash player@")+"" );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash player&')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%26",  encodeURIComponent("http://www.macromedia.com/flash player&")+"" );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash player=')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3D",  encodeURIComponent("http://www.macromedia.com/flash player=")+"" );

    array[item++] = new TestCase( SECTION,  "encodeURIComponent('http://www.macromedia.com/flash player$')", "http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%24",  encodeURIComponent("http://www.macromedia.com/flash player$")+"" );

    return ( array );
}
