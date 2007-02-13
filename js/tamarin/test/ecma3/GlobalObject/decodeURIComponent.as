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

    var SECTION = "15.1.3.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "decodeURIComponent";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array:Array = new Array();
    var item:Number= 0;

    var str1:String = new String("h");

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('Hello%7B%5BWorld%5D%7D')", "Hello{[World]}",  decodeURIComponent("Hello%7B%5BWorld%5D%7D") );
    array[item++] = new TestCase( SECTION,  "decodeURIComponent('Macromedia%20-%20Flash')", "Macromedia - Flash",  decodeURIComponent("Macromedia%20-%20Flash") );
    array[item++] = new TestCase( SECTION,  "decodeURIComponent('2%20*%204%20%2B%20%5B8%20%2B%205%20%5D%20-%203')", "2 * 4 + [8 + 5 ] - 3",  decodeURIComponent("2%20*%204%20%2B%20%5B8%20%2B%205%20%5D%20-%203") );
    array[item++] = new TestCase( SECTION,  "decodeURIComponent('Flash(Macromedia)')", "Flash(Macromedia)",  decodeURIComponent("Flash(Macromedia)") );
    array[item++] = new TestCase( SECTION,  "decodeURIComponent('BugID%20%2317485')", "BugID #17485",  decodeURIComponent("BugID%20%2317485") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflasha%20player')", "http://www.macromedia.com/flasha player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflasha%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2FflashA%20player')", "http://www.macromedia.com/flashA player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2FflashA%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash_%20player')", "http://www.macromedia.com/flash_ player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash_%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash-%20player')", "http://www.macromedia.com/flash- player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash-%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash.%20player')", "http://www.macromedia.com/flash. player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash.%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash!%20player')", "http://www.macromedia.com/flash! player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash!%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash~%20player')", "http://www.macromedia.com/flash~ player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash~%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash*%20player')", "http://www.macromedia.com/flash* player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash*%20player") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2F'flash%20player'')", "http://www.macromedia.com/'flash player'",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2F'flash%20player'") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2F(flash%20player)')", "http://www.macromedia.com/(flash player)",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2F(flash%20player)") );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%3B%20player')", "http://www.macromedia.com/flash; player",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%3B%20player")+"" );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3F')", "http://www.macromedia.com/flash player?",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3F")+"" );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%40')", "http://www.macromedia.com/flash player@",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%40")+"" );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%26')", "http://www.macromedia.com/flash player&",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%26")+"" );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3D')", "http://www.macromedia.com/flash player=",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%3D")+"" );

    array[item++] = new TestCase( SECTION,  "decodeURIComponent('http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%24')", "http://www.macromedia.com/flash player$",  decodeURIComponent("http%3A%2F%2Fwww.macromedia.com%2Fflash%20player%24")+"" );

    var thisError:String = 'no exception';
    try{
        decodeURIComponent('http://www.macromedia.com/flash%GKplayer')
    }catch(e:Error){
        thisError=(e.toString()).substring(0,8);
    }finally{
        array[item++] = new TestCase( SECTION,  "Characters following % should be hexa decimal digits", "URIError",  thisError);
    }


   thisError = 'no exception';
   try{
       decodeURIComponent('http://www.macromedia.com/flash%20player%')
   }catch(e1:Error){
       thisError=(e1.toString()).substring(0,8);
   }finally{
       array[item++] = new TestCase( SECTION,  "If the last character of string is % throw URIError", "URIError",  thisError);
   }

   thisError = 'no exception';
   try{
       decodeURIComponent('http://www.macromedia.com/flash5%2player')
   }catch(e2:Error){
       thisError=(e2.toString()).substring(0,8);
   }finally{
       array[item++] = new TestCase( SECTION,  "If the character at position k  of string before hexadecimal digits is not % throw URIError", "URIError",  thisError);
   }


    return ( array );
}
