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
     use namespace AS3;
    var SECTION = "15.5.3.2-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.fromCharCode()";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
 
    var array = new Array();
    var item = 0;
    //var x = new String();
    var thisError="no error";
    array[item++] = new TestCase( SECTION,   "typeof String.fromCharCode",      "function", typeof String.fromCharCode );  
    array[item++] = new TestCase( SECTION,   "typeof String.prototype.fromCharCode",        "undefined", typeof String.prototype.fromCharCode );
    try{
        var x = new String();
        x.fromCharcode;
    }catch(e:ReferenceError){
        thisError=(e.toString()).substring(0,27);
    }finally{
        array[item++] = new TestCase( SECTION,   "var x = new String(), typeof x.fromCharCode","ReferenceError: Error #1069",thisError);
    }
 /*array[item++] = new TestCase( SECTION,   "var x = new String(), typeof x.fromCharCode","undefined",typeof x.fromCharCode  );*/
    array[item++] = new TestCase( SECTION,   "String.fromCharCode.length",      0,      String.fromCharCode.length );
    array[item++] = new TestCase( SECTION,    "String.fromCharCode()",          "",     String.fromCharCode() );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0020)",     " ",   String.fromCharCode(0x0020) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0021)",     "!",   String.fromCharCode(0x0021) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0022)",     "\"",   String.fromCharCode(0x0022) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0023)",     "#",   String.fromCharCode(0x0023) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0024)",     "$",   String.fromCharCode(0x0024) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0025)",     "%",   String.fromCharCode(0x0025) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0026)",     "&",   String.fromCharCode(0x0026) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0027)",     "\'",   String.fromCharCode(0x0027) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0028)",     "(",   String.fromCharCode(0x0028) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0029)",     ")",   String.fromCharCode(0x0029) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002A)",     "*",   String.fromCharCode(0x002A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002B)",     "+",   String.fromCharCode(0x002B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002C)",     ",",   String.fromCharCode(0x002C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002D)",     "-",   String.fromCharCode(0x002D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002E)",     ".",   String.fromCharCode(0x002E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x002F)",     "/",   String.fromCharCode(0x002F) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0030)",     "0",   String.fromCharCode(0x0030) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0031)",     "1",   String.fromCharCode(0x0031) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0032)",     "2",   String.fromCharCode(0x0032) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0033)",     "3",   String.fromCharCode(0x0033) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0034)",     "4",   String.fromCharCode(0x0034) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0035)",     "5",   String.fromCharCode(0x0035) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0036)",     "6",   String.fromCharCode(0x0036) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0037)",     "7",   String.fromCharCode(0x0037) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0038)",     "8",   String.fromCharCode(0x0038) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0039)",     "9",   String.fromCharCode(0x0039) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003A)",     ":",   String.fromCharCode(0x003A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003B)",     ";",   String.fromCharCode(0x003B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003C)",     "<",   String.fromCharCode(0x003C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003D)",     "=",   String.fromCharCode(0x003D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003E)",     ">",   String.fromCharCode(0x003E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x003F)",     "?",   String.fromCharCode(0x003F) );

    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0040)",     "@",   String.fromCharCode(0x0040) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0041)",     "A",   String.fromCharCode(0x0041) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0042)",     "B",   String.fromCharCode(0x0042) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0043)",     "C",   String.fromCharCode(0x0043) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0044)",     "D",   String.fromCharCode(0x0044) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0045)",     "E",   String.fromCharCode(0x0045) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0046)",     "F",   String.fromCharCode(0x0046) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0047)",     "G",   String.fromCharCode(0x0047) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0048)",     "H",   String.fromCharCode(0x0048) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0049)",     "I",   String.fromCharCode(0x0049) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004A)",     "J",   String.fromCharCode(0x004A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004B)",     "K",   String.fromCharCode(0x004B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004C)",     "L",   String.fromCharCode(0x004C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004D)",     "M",   String.fromCharCode(0x004D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004E)",     "N",   String.fromCharCode(0x004E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004F)",     "O",   String.fromCharCode(0x004F) );

    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0040)",     "@",   String.fromCharCode(0x0040) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0041)",     "A",   String.fromCharCode(0x0041) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0042)",     "B",   String.fromCharCode(0x0042) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0043)",     "C",   String.fromCharCode(0x0043) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0044)",     "D",   String.fromCharCode(0x0044) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0045)",     "E",   String.fromCharCode(0x0045) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0046)",     "F",   String.fromCharCode(0x0046) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0047)",     "G",   String.fromCharCode(0x0047) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0048)",     "H",   String.fromCharCode(0x0048) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0049)",     "I",   String.fromCharCode(0x0049) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004A)",     "J",   String.fromCharCode(0x004A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004B)",     "K",   String.fromCharCode(0x004B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004C)",     "L",   String.fromCharCode(0x004C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004D)",     "M",   String.fromCharCode(0x004D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004E)",     "N",   String.fromCharCode(0x004E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x004F)",     "O",   String.fromCharCode(0x004F) );

    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0050)",     "P",   String.fromCharCode(0x0050) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0051)",     "Q",   String.fromCharCode(0x0051) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0052)",     "R",   String.fromCharCode(0x0052) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0053)",     "S",   String.fromCharCode(0x0053) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0054)",     "T",   String.fromCharCode(0x0054) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0055)",     "U",   String.fromCharCode(0x0055) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0056)",     "V",   String.fromCharCode(0x0056) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0057)",     "W",   String.fromCharCode(0x0057) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0058)",     "X",   String.fromCharCode(0x0058) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0059)",     "Y",   String.fromCharCode(0x0059) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005A)",     "Z",   String.fromCharCode(0x005A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005B)",     "[",   String.fromCharCode(0x005B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005C)",     "\\",   String.fromCharCode(0x005C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005D)",     "]",   String.fromCharCode(0x005D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005E)",     "^",   String.fromCharCode(0x005E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x005F)",     "_",   String.fromCharCode(0x005F) );

    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0060)",     "`",   String.fromCharCode(0x0060) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0061)",     "a",   String.fromCharCode(0x0061) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0062)",     "b",   String.fromCharCode(0x0062) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0063)",     "c",   String.fromCharCode(0x0063) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0064)",     "d",   String.fromCharCode(0x0064) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0065)",     "e",   String.fromCharCode(0x0065) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0066)",     "f",   String.fromCharCode(0x0066) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0067)",     "g",   String.fromCharCode(0x0067) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0068)",     "h",   String.fromCharCode(0x0068) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0069)",     "i",   String.fromCharCode(0x0069) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006A)",     "j",   String.fromCharCode(0x006A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006B)",     "k",   String.fromCharCode(0x006B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006C)",     "l",   String.fromCharCode(0x006C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006D)",     "m",   String.fromCharCode(0x006D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006E)",     "n",   String.fromCharCode(0x006E) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x006F)",     "o",   String.fromCharCode(0x006F) );

    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0070)",     "p",   String.fromCharCode(0x0070) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0071)",     "q",   String.fromCharCode(0x0071) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0072)",     "r",   String.fromCharCode(0x0072) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0073)",     "s",   String.fromCharCode(0x0073) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0074)",     "t",   String.fromCharCode(0x0074) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0075)",     "u",   String.fromCharCode(0x0075) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0076)",     "v",   String.fromCharCode(0x0076) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0077)",     "w",   String.fromCharCode(0x0077) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0078)",     "x",   String.fromCharCode(0x0078) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0079)",     "y",   String.fromCharCode(0x0079) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x007A)",     "z",   String.fromCharCode(0x007A) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x007B)",     "{",   String.fromCharCode(0x007B) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x007C)",     "|",   String.fromCharCode(0x007C) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x007D)",     "}",   String.fromCharCode(0x007D) );
    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x007E)",     "~",   String.fromCharCode(0x007E) );
//    array[item++] = new TestCase( SECTION,   "String.fromCharCode(0x0020, 0x007F)",     "",   String.fromCharCode(0x0040, 0x007F) );

    return array;
}

