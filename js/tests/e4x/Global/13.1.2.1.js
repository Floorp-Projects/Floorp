/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
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

START("13.1.2.1 - isXMLName()");

TEST(1, true, typeof isXMLName == "function");


// Check converting to string
var object = { toString: function() { return "text"; } };

TEST(2, true, isXMLName(object));

// This throws TypeError => isXMLName should give false
var object2 = { toString: function() { return this; } };
TEST(3, false, isXMLName(object2));

// Check inderect throw of TypeError
var object3 = { toString: function() { return String(object2); } };
TEST(3, false, isXMLName(object3));

var object4 = { toString: function() { throw 1; } };
try {
    isXMLName(object4);
    SHOULD_THROW(4);
} catch (e) {
    TEST(4, 1, e);
}

// Check various cases of http://w3.org/TR/xml-names11/#NT-NCName

TEST(5, false, isXMLName(""));

// fill buffer with allowed codes up to 0xFFFF
var buffer = [];

buffer.push("_".charCodeAt(0));

pushInterval(buffer, "A".charCodeAt(0), "Z".charCodeAt(0));
pushInterval(buffer, "a".charCodeAt(0), "z".charCodeAt(0));
pushInterval(buffer, 0xC0, 0xD6); 
pushInterval(buffer, 0xD8, 0xF6); 
pushInterval(buffer, 0xF8, 0x2FF); 
pushInterval(buffer, 0x370, 0x37D); 
pushInterval(buffer, 0x37F, 0x1FFF); 
pushInterval(buffer, 0x200C, 0x200D); 
pushInterval(buffer, 0x2070, 0x218F); 
pushInterval(buffer, 0x2C00, 0x2FEF); 
pushInterval(buffer, 0x3001, 0xD7FF); 
pushInterval(buffer, 0xF900, 0xFDCF); 
pushInterval(buffer, 0xFDF0, 0xFFFD); 


var validStartCharsEnd = buffer.length;

pushInterval(buffer, "0".charCodeAt(0), "9".charCodeAt(0));
buffer.push("-".charCodeAt(0));
buffer.push(".".charCodeAt(0));
buffer.push(0xB7);

pushInterval(buffer, 0x0300, 0x036F);
pushInterval(buffer, 0x203F, 0x2040);

var stringWithAllValidCahrsBelow64K = String.fromCharCode.apply(this, buffer);

// Self-consistency test
TEST(6, stringWithAllValidCahrsBelow64K.length, buffer.length);

// Check that string with all valid characters bellow 64K gives true
TEST(7, true, isXMLName(stringWithAllValidCahrsBelow64K));

// Return string with all failed char indexes.
function getBadIndexes()
{
    // This has to be optimized or it would take too long time to run in Rhino    
    var x_code = "x".charCodeAt(0);
    var validStartCharsBelow64K = {};
    var validNonStartCharsBelow64K = {};
    
    var badIndexes = [];
    var i, end = +validStartCharsEnd;
    for (i = 0; i != end; ++i) {
        validStartCharsBelow64K[buffer[i]] = true;
    }

    end = +buffer.length;
    for (; i != end; ++i) {
        validNonStartCharsBelow64K[buffer[i]] = true;
    }

    var isName = isXMLName;
    var fromCharCode = String.fromCharCode;
    for (i = 0; i != 0xFFFF; ++i) {
        var s = fromCharCode(i);
        if (i in validStartCharsBelow64K) {
            if (!isName(s)) {
                badIndexes.push(i);
            }  
        } else if (i in validNonStartCharsBelow64K) {
            if (isName(s)) {
                badIndexes.push(i);
            }  
        } else {
            if (isName(s) || isName(fromCharCode(x_code, i))) {
                badIndexes.push(i);
            }  
        }
    }
    
    var result = "";
    for (var i = 0; i != badIndexes.length; ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += "0x"+badIndexes[i].toString(16);
    }
    
    return result;
}

TEST(8, "", getBadIndexes());

END();


// Utilities

function pushInterval(array, from, to)
{
    if (from > to) throw "Bad arg";
    var l = array.length;
    while (from <= to) {
        array[l++] = from++;
    }
}
