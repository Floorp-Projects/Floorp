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

    var SECTION = "15.1.3.1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "decodeURI";

    writeHeaderToLog( SECTION + " "+ TITLE);
    var testcases = getTestCases();
    test();

function getTestCases() {
    var array:Array = new Array();
    var item:Number = 0;

    var str1 = new String("h");

    array[item++] = new TestCase( SECTION,  "decodeURI('')", "",  decodeURI("") );

    array[item++] = new TestCase( SECTION,  "decodeURI(str1)", "h",  decodeURI(str1) );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flashA1player')", "http://www.macromedia.com/flash\u0041\u0031player",  decodeURI("http://www.macromedia.com/flashA1player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flasha%20player')", "http://www.macromedia.com/flasha player",  decodeURI("http://www.macromedia.com/flasha%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flashA%20player')", "http://www.macromedia.com/flashA player",  decodeURI("http://www.macromedia.com/flashA%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash_%20player')", "http://www.macromedia.com/flash_ player",  decodeURI("http://www.macromedia.com/flash_%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash-%20player')", "http://www.macromedia.com/flash- player",  decodeURI("http://www.macromedia.com/flash-%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash.%20player')", "http://www.macromedia.com/flash. player",  decodeURI("http://www.macromedia.com/flash.%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash!%20player')", "http://www.macromedia.com/flash!  player",  decodeURI("http://www.macromedia.com/flash!%20 player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash~%20player')", "http://www.macromedia.com/flash~ player",  decodeURI("http://www.macromedia.com/flash~%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash*%20player')", "http://www.macromedia.com/flash* player",  decodeURI("http://www.macromedia.com/flash*%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/'flash%20player'')", "http://www.macromedia.com/'flash player'",  decodeURI("http://www.macromedia.com/'flash%20player'") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/(flash%20player)')", "http://www.macromedia.com/(flash player)",  decodeURI("http://www.macromedia.com/(flash%20player)") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/%09flash%20player", "http://www.macromedia.com/\tflash player",  decodeURI("http://www.macromedia.com/%09flash%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/0987654321flash%20player')", "http://www.macromedia.com/0987654321flash player",  decodeURI("http://www.macromedia.com/0987654321flash%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash;%20player')", "http://www.macromedia.com/flash; player",  decodeURI("http://www.macromedia.com/flash;%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player?')", "http://www.macromedia.com/flash player?",  decodeURI("http://www.macromedia.com/flash%20player?") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player@')", "http://www.macromedia.com/flash player@",  decodeURI("http://www.macromedia.com/flash%20player@") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player&')", "http://www.macromedia.com/flash player&",  decodeURI("http://www.macromedia.com/flash%20player&") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player=')", "http://www.macromedia.com/flash player=",  decodeURI("http://www.macromedia.com/flash%20player=") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player$')", "http://www.macromedia.com/flash player$",  decodeURI("http://www.macromedia.com/flash%20player$") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player,')", "http://www.macromedia.com/flash player,",  decodeURI("http://www.macromedia.com/flash%20player,") );

    array[item++] = new TestCase( SECTION,  "decodeURI('aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ')", "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ",  decodeURI("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ") );

    array[item++] = new TestCase( SECTION,  "decodeURI('aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ')", "aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ",  decodeURI("aA_bB-cC.dD!eE~fF*gG'hH(iI)jJ;kK/lL?mM:nN@oO&pP=qQ+rR$sS,tT9uU8vV7wW6xX5yY4zZ") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%0Aplayer')", "http://www.macromedia.com/flash\nplayer",  decodeURI("http://www.macromedia.com/flash%0Aplayer") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%0Bplayer')", "http://www.macromedia.com/flash\vplayer",  decodeURI("http://www.macromedia.com/flash%0Bplayer") );


   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%0Cplayer')", "http://www.macromedia.com/flash\fplayer",  decodeURI("http://www.macromedia.com/flash%0Cplayer") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%0Dplayer')", "http://www.macromedia.com/flash\rplayer",  decodeURI("http://www.macromedia.com/flash%0Dplayer") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%22player')", "http://www.macromedia.com/flash\"player",  decodeURI("http://www.macromedia.com/flash%22player") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%27player')", "http://www.macromedia.com/flash\'player",  decodeURI("http://www.macromedia.com/flash%27player") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%5Cplayer')", "http://www.macromedia.com/flash\\player",  decodeURI("http://www.macromedia.com/flash%5Cplayer") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash#player')", "http://www.macromedia.com/flash#player",  decodeURI("http://www.macromedia.com/flash#player") );

   array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%00Aplayer')", "http://www.macromedia.com/flash\u0000\u0041player",  decodeURI("http://www.macromedia.com/flash%00Aplayer") );


    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com')", "http://www.macromedia.com",  decodeURI("http://www.macromedia.com") );

    array[item++] = new TestCase( SECTION,  "decodeURI('http://www.macromedia.com/flash%20player')", "http://www.macromedia.com/flash player",  decodeURI("http://www.macromedia.com/flash%20player") );

    var thisError:String = 'no exception';
   try{
       decodeURI('http://www.macromedia.com/flash%GKplayer')
   }catch(e:Error){
       thisError=(e.toString()).substring(0,8);
   }finally{
       array[item++] = new TestCase( SECTION,  "Characters following % should be hexa decimal digits", "URIError",  thisError);
   }


   thisError = 'no exception';
   try{
       decodeURI('http://www.macromedia.com/flash%20player%')
   }catch(e1:Error){
       thisError=(e1.toString()).substring(0,8);
   }finally{
       array[item++] = new TestCase( SECTION,  "If the last character of string is % throw URIError", "URIError",  thisError);
   }

   thisError = 'no exception';
   try{
       decodeURI('http://www.macromedia.com/flash5%2player')
   }catch(e2:Error){
       thisError=(e2.toString()).substring(0,8);
   }finally{
       array[item++] = new TestCase( SECTION,  "If the character at position k  of string is not % throw URIError", "URIError",  thisError);
   }

    return ( array );
}
