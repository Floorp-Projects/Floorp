/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Rob Ginda rginda@netscape.com
 */

var to = new Object();
to.z getter= function () {
    return "zee";
    }

test();
    
function TestObject ()
{
    /* A warm, dry place for properties and methods to live */
}

TestObject.prototype._y = "<initial y>";
    
TestObject.prototype.y getter= function()
{
    var rv;
    
    reportFailure ("***************");
    throw "foo";
    if (typeof this._y == "string")
        rv = "got " + this._y;
    else
        rv = this._y;
    
    return rv;
}

TestObject.prototype.y setter=function set_y (newVal)
{
    reportFailure ("*************** setter");

    this._y = newVal;
}


function test()
{
    enterFunc ("test");
    
    printStatus ("Basic Getter/ Setter test");

    reportCompare ("zee", to.z, "z getter, before set");

    var t = new TestObject();

    reportCompare ("got <initial y>", t.y, "y getter, before set");

    t.y = "new y";
    reportCompare ("got new y", t.y, "y getter, after set");

    t.y = 2;
    reportCompare (2, t.y, "y getter, after numeric set");

    var d = new Date();
    t.y = d;
    reportCompare (d, t.y, "y getter, after date set");


//    reportCompare (get_y, t.y, "y getter, after numeric set");

}

    
    

    

        
        
    
