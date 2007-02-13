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

    var SECTION = "15.1.3.3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "encodeURI";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    str1 = new String("h");

    array[item++] = new TestCase( SECTION,  "encodeURI('Empty String')", "",  encodeURI("") );
    array[item++] = new TestCase( SECTION,  "encodeURI('Single Character')", "h",  encodeURI(str1) );

    str2 = new String("http://www.macromedia.com/flash player");

    array[item++] = new TestCase( SECTION,  "encodeURI(str2)", "http://www.macromedia.com/flash%20player",  encodeURI(str2) );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com')", "http://www.macromedia.com",  encodeURI("http://www.macromedia.com") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com')", "http://www.macromedia.com/flashA1player",  encodeURI("http://www.macromedia.com/flash\u0041\u0031player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player')", "http://www.macromedia.com/flash%20player",  encodeURI("http://www.macromedia.com/flash player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flasha player')", "http://www.macromedia.com/flasha%20player",  encodeURI("http://www.macromedia.com/flasha player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flashA player')", "http://www.macromedia.com/flashA%20player",  encodeURI("http://www.macromedia.com/flashA player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash_ player')", "http://www.macromedia.com/flash_%20player",  encodeURI("http://www.macromedia.com/flash_ player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash- player')", "http://www.macromedia.com/flash-%20player",  encodeURI("http://www.macromedia.com/flash- player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash. player')", "http://www.macromedia.com/flash.%20player",  encodeURI("http://www.macromedia.com/flash. player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash! player')", "http://www.macromedia.com/flash!%20player",  encodeURI("http://www.macromedia.com/flash! player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash~ player')", "http://www.macromedia.com/flash~%20player",  encodeURI("http://www.macromedia.com/flash~ player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash* player')", "http://www.macromedia.com/flash*%20player",  encodeURI("http://www.macromedia.com/flash* player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/'flash player'')", "http://www.macromedia.com/'flash%20player'",  encodeURI("http://www.macromedia.com/'flash player'") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/(flash player)')", "http://www.macromedia.com/(flash%20player)",  encodeURI("http://www.macromedia.com/(flash player)") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com//tflash player')", "http://www.macromedia.com//tflash%20player",  encodeURI("http://www.macromedia.com//tflash player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/\tflash player')", "http://www.macromedia.com/%09flash%20player",  encodeURI("http://www.macromedia.com/\tflash player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/0987654321flash player')", "http://www.macromedia.com/0987654321flash%20player",  encodeURI("http://www.macromedia.com/0987654321flash player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash; player')", "http://www.macromedia.com/flash;%20player",  encodeURI("http://www.macromedia.com/flash; player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player?')", "http://www.macromedia.com/flash%20player?",  encodeURI("http://www.macromedia.com/flash player?") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player@')", "http://www.macromedia.com/flash%20player@",  encodeURI("http://www.macromedia.com/flash player@") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player&')", "http://www.macromedia.com/flash%20player&",  encodeURI("http://www.macromedia.com/flash player&") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player=')", "http://www.macromedia.com/flash%20player=",  encodeURI("http://www.macromedia.com/flash player=") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player$')", "http://www.macromedia.com/flash%20player$",  encodeURI("http://www.macromedia.com/flash player$") );


    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash player,')", "http://www.macromedia.com/flash%20player,",  encodeURI("http://www.macromedia.com/flash player,") );


    array[item++] = new TestCase( SECTION,  "encodeURI('aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ')", "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ",  encodeURI("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ") );

    array[item++] = new TestCase( SECTION,  "encodeURI('aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ')", "aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ",  encodeURI("aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\n player,')", "http://www.macromedia.com/flash%0Aplayer",  encodeURI("http://www.macromedia.com/flash\nplayer") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\v player,')", "http://www.macromedia.com/flash%0Bplayer",  encodeURI("http://www.macromedia.com/flash\vplayer") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\f player,')", "http://www.macromedia.com/flash%0Cplayer",  encodeURI("http://www.macromedia.com/flash\fplayer") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\r player,')", "http://www.macromedia.com/flash%0Dplayer",  encodeURI("http://www.macromedia.com/flash\rplayer") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\" player,')", "http://www.macromedia.com/flash%22player",  encodeURI("http://www.macromedia.com/flash\"player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\' player,')", "http://www.macromedia.com/flash'player",  encodeURI("http://www.macromedia.com/flash\'player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\\ player,')", "http://www.macromedia.com/flash%5Cplayer",  encodeURI("http://www.macromedia.com/flash\\player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash# player,')", "http://www.macromedia.com/flash#player",  encodeURI("http://www.macromedia.com/flash#player") );

    array[item++] = new TestCase( SECTION,  "encodeURI('http://www.macromedia.com/flash\u0000\u0041player,')", "http://www.macromedia.com/flash%00Aplayer",  encodeURI("http://www.macromedia.com/flash\u0000\u0041player") );

    return ( array );
}
