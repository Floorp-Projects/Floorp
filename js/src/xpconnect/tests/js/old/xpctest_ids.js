/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* This is a test for nsIIDs and nsCIDs in JavaScript. */

// our standard print function
function println(s)
{
    if(this.document === undefined)
        print(s);
    else
        dump(s+"\n");
}

// alias this ctor for simplicity of use
var nsID = Components.ID;

// some explicit IDs

var NS_ISUPPORTS_IID = new nsID("{00000000-0000-0000-c000-000000000046}");
var NS_ECHO          = new nsID("{ed132c20-eed1-11d2-baa4-00805f8a5dd7}");
var NS_ECHO_UPPER    = new nsID("{ED132C20-EED1-11D2-BAA4-00805F8A5DD7}");
var NS_BOGUS_IID     = new nsID("{15898420-4d11-11d3-9893-006008962422}");

// our test data array
data = [
 // toString of bare id resolved to canonical form
 [NS_ISUPPORTS_IID                   , "{00000000-0000-0000-c000-000000000046}"],
 // name of bare id is empty string
 [NS_ISUPPORTS_IID.name              , ""],
 // number of bare id resolved to canonical form
 [NS_ISUPPORTS_IID.number            , "{00000000-0000-0000-c000-000000000046}"],
 // this one is made up so it only resolves to the number
 [NS_BOGUS_IID                       , "{15898420-4d11-11d3-9893-006008962422}"],

 // now we check those the 'Components' knows about
 [Components.interfaces.nsISupports  , "nsISupports"],
 [Components.classes["@mozilla.org/js/xpc/test/Echo;1"]
                                     , "@mozilla.org/js/xpc/test/Echo;1"],
 [Components.classes["@mozilla.org/js/xpc/test/Echo;1"].number   
                                     , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],

 // now a bogus one
 [Components.interfaces.bogus        , "undefined"],

 // even though these 'exist', they are not addressable by canonical name
 [Components.interfaces["{00000000-0000-0000-c000-000000000046}"] , "undefined"],
 [Components.classes["@mozilla.org/js/xpc/test/Echo;1"]["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"], "undefined"],

 // we *don't* expect bare CLSIDs to resolve to contractids    
 [NS_ECHO_UPPER                      , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],
 [NS_ECHO                            , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],
 [NS_ECHO_UPPER.number               , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],
 [NS_ECHO.number                     , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],

 // classesByID requires canonical numbers
 [Components.classesByID.nsEcho      , "undefined"],
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],       "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"].number, "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],

 // works with valid CID
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"], "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],

 // does not work with bogus CID
 [Components.classesByID["{35fb7000-4d23-11d3-9893-006008962422}"], "undefined"],

 // classesByID should not resolve the number to the contractid
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"].name,  ""],
 // ...though it is clearly the same object...
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"].equals(
                    Components.classes["@mozilla.org/js/xpc/test/Echo;1"]), true],

 // equals self
 [Components.classes["@mozilla.org/js/xpc/test/Echo;1"].equals(
                    Components.classes["@mozilla.org/js/xpc/test/Echo;1"]), true],

 // we could add tests for more of the ID properties here...

]

// do the tests

println("\nJavaScript nsID tests...\n");
var failureCount = 0;
for(var i = 0; i < data.length; i++) {
    var item = data[i];
    if(""+item[0] != ""+item[1]) {
        println("failed for item "+i+" expected \""+item[1]+"\" got \""+item[0]+"\"");
        failureCount ++;
    }
}

// show the final results

println("");
if(failureCount)
    println(failureCount+" FAILURES total");
else
    println("all PASSED");
