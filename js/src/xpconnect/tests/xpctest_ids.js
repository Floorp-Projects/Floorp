/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* This is a test for nsIIDs and nsCIDs in JavaScript. */

// our standard print function
function println(s)
{
    if(typeof(this.document) == "undefined")
        print(s);
    else
        dump(s+"\n");
}

// a ctor for Interface IDs
function nsIID(str)
{
    var id = Components.classes.nsIID.createInstance();
    id = id.QueryInterface(Components.interfaces.nsIJSIID);
    id.init(str);
    return id;
}

// a ctor for CLSIDs
function nsCID(str)
{
    var id = Components.classes.nsCID.createInstance();
    id = id.QueryInterface(Components.interfaces.nsIJSCID);
    id.init(str);
    return id;
}

// some explicit IDs

var NS_ISUPPORTS_IID = new nsIID("{00000000-0000-0000-c000-000000000046}");
var NS_ECHO_UPPER    = new nsCID("{ED132C20-EED1-11D2-BAA4-00805F8A5DD7}");
var NS_ECHO          = new nsCID("{ed132c20-eed1-11d2-baa4-00805f8a5dd7}");
var NS_BOGUS_IID     = new nsIID("{15898420-4d11-11d3-9893-006008962422}");

// our test data array
data = [
 // we expect bare interfacesIDs to resolve to names of interfaces (if they exist)
 [NS_ISUPPORTS_IID                   , "nsISupports"],
 [NS_ISUPPORTS_IID.name              , "nsISupports"],
 [NS_ISUPPORTS_IID.number            , "{00000000-0000-0000-c000-000000000046}"],
 // this one is made up so it only resolves to the number
 [NS_BOGUS_IID                       , "{15898420-4d11-11d3-9893-006008962422}"],

 // now we check those the 'Components' knows about
 [Components.interfaces.nsISupports  , "nsISupports"],
 [Components.classes.nsEcho          , "nsEcho"],
 [Components.classes.nsEcho.number   , "{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"],

 // now a bogus one
 [Components.interfaces.bogus        , "undefined"],

 // even though these 'exist', they are not addressable by canonical name
 [Components.interfaces["{00000000-0000-0000-c000-000000000046}"] , "undefined"],
 [Components.classes.nsEcho["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"], "undefined"],

 // we *don't* expect bare CLSIDs to resolve to progids    
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

 // but the same bogus CID works if not claiming that it is a registered CID
 [new nsCID("{35fb7000-4d23-11d3-9893-006008962422}"), "{35fb7000-4d23-11d3-9893-006008962422}"],

 // classesByID should not resolve the number to the progid
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"].name,  ""],
 // ...though it is clearly the same object...
 [Components.classesByID["{ed132c20-eed1-11d2-baa4-00805f8a5dd7}"].equals(Components.classes.nsEcho), true],

 // equals self
 [Components.classes.nsEcho.equals(Components.classes.nsEcho), true],

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
