/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// get title directly
self.postMessage(["equal", document.title, "Page Worker test",
            "Correct page title accessed directly"]);

// get <p> directly
let p = document.getElementById("paragraph");
self.postMessage(["ok", !!p, "<p> can be accessed directly"]);
self.postMessage(["equal", p.firstChild.nodeValue,
            "Lorem ipsum dolor sit amet.",
            "Correct text node expected"]);

// Modify page
let div = document.createElement("div");
div.setAttribute("id", "block");
div.appendChild(document.createTextNode("Test text created"));
document.body.appendChild(div);

// Check back the modification
div = document.getElementById("block");
self.postMessage(["ok", !!div, "<div> can be accessed directly"]);
self.postMessage(["equal", div.firstChild.nodeValue,
            "Test text created", "Correct text node expected"]);
self.postMessage(["done"]);

