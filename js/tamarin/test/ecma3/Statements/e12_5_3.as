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
  

    var SECTION = "12.5";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The if statment";

    writeHeaderToLog( SECTION + " "+ TITLE);


    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        
    var MYVAR; 
    if ( true ) 
        MYVAR='PASSED'; 
    else if(false)
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( true ) MYVAR='PASSED'; else if (false) MYVAR= 'FAILED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR; 
    if ( false ) 
        MYVAR='FAILED'; 
    else if (true)
        MYVAR= 'PASSED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( false ) MYVAR='FAILED'; else if (true) MYVAR= 'PASSED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR; 
    if ( new Boolean(true) )
        MYVAR='PASSED'; 
    else if (new Boolean(false))
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( new Boolean(true) )  MYVAR='PASSED'; else if (new Boolean(false))MYVAR= 'FAILED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR; 
    if ( new Boolean(false) ) 
        MYVAR='PASSED'; 
    else if (new Boolean(true))
        MYVAR= 'FAILED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; else if (new Boolean(true)) MYVAR= 'FAILED';",
                                    "FAILED",
                                    MYVAR);
    var MYVAR;
    if ( 1 ) 
        MYVAR='PASSED';
    else if (0)
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( 1 ) MYVAR='PASSED'; else if (0) MYVAR= 'FAILED';",
                                    "PASSED",
                                    MYVAR);
	var MYVAR; 
	if ( 0 ) 
        MYVAR='FAILED'; 
	else if (1)
        MYVAR= 'PASSED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( 0 ) MYVAR='FAILED'; else if (1) MYVAR= 'PASSED';","PASSED",MYVAR);

    var MyVar1 = 50;
    var MyVar2 = 100;

    if (MyVar1>MyVar2)
        result="MyVar1 is greater than MyVar2";
    else if (MyVar2==MyVar1) 
        result="MyVar2 equals MyVar1";
    else
        result="MyVar2 greater than MyVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyVar2 greater than MyVar1",result);

    var MyVar1 = 100;
    var MyVar2 = 50;

    if (MyVar1>MyVar2)
        result="MyVar1 is greater than MyVar2";
    else if (MyVar2==MyVar1) 
        result="MyVar2 equals MyVar1";
    else
        result="MyVar2 greater than MyVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyVar1 is greater than MyVar2",result);

    var MyVar1 = 50;
    var MyVar2 = 50;

    if (MyVar1>MyVar2)
        result="MyVar1 is greater than MyVar2";
    else if (MyVar2==MyVar1) 
        result="MyVar2 equals MyVar1";
    else
        result="MyVar2 greater than MyVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyVar2 equals MyVar1",result);


    var MyStringVar1 = "string"
    var MyStringVar2 = "string";

    if (MyStringVar1>MyStringVar2)
        result="MyStringVar1 is greater than MyStringVar2";
    else if (MyStringVar2==MyStringVar1) 
        result="MyStringVar2 equals MyStringVar1";
    else
        result="MyStringVar2 greater than MyStringVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyStringVar2 equals MyStringVar1",result);

    var MyStringVar1 = "String";
    var MyStringVar2 = "string";

    if (MyStringVar1>MyStringVar2)
        result="MyStringVar1 is greater than MyStringVar2";
    else if (MyStringVar2==MyStringVar1) 
        result="MyStringVar2 equals MyStringVar1";
    else
        result="MyStringVar2 greater than MyStringVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyStringVar2 greater than MyStringVar1",result);


    var MyStringVar1 = "strings";
    var MyStringVar2 = "string";

    if (MyStringVar1>MyStringVar2)
        result="MyStringVar1 is greater than MyStringVar2";
    else if (MyStringVar2==MyStringVar1) 
        result="MyStringVar2 equals MyStringVar1";
    else
        result="MyStringVar2 greater than MyStringVar1";

    array[item++] = new TestCase(  SECTION,"Testing if elseif else","MyStringVar1 is greater than MyStringVar2",result);

    return array;
}
