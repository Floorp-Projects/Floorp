/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//
// travers the html tree and dump out the type of element
//
function traverse(node, indent)
{
    dump("\n")
    indent += "  "
    var type = node.nodeType;

    // if it's an element dump the tag and recurse the children
    if (type == Node.ELEMENT_NODE) {

        dump(indent + node.tagName)

        // go through the children
        if (node.hasChildNodes()) {
            var children = node.childNodes;
            var length = children.length;
            var count = 0;
            while(count < length) {
                child = children[count]
                traverse(child, indent)
                count++
            }
        }
    }
    // it's just text, no tag, dump "Text"
    else if (type == Node.TEXT_NODE) {
        dump(indent + "Text")
    }
}

var node = document.documentElement

traverse(node, "")
dump("\n")

  