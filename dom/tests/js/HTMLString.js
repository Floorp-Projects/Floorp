/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//
// return a string representing the html content in html format 
//
function htmlString(node, indent)
{
    var html = ""
    indent += "  "

    var type = node.nodeType
    if (type == Node.ELEMENT) {

        // open tag
        html += "\n" + indent + "<" + node.tagName

        // dump the attributes if any
        attributes = node.attributes
        if (null != attributes) {
            var countAttrs = attributes.length
            var index = 0
            while(index < countAttrs) {
                att = attributes[index]
                if (null != att) {
                    html += " "
                    html += att.name + "=" + att.value;
                }
                index++
            }
        }

        // end tag
        html += ">"

        // recursively dump the children
        if (node.hasChildNodes) {
            // get the children
            var children = node.childNodes
            var length = children.length
            var count = 0;
            while(count < length) {
                child = children[count]
                html += htmlString(child, indent)
                count++
            }
        }

        // close tag
        html += "\n" + indent + "</" + node.tagName + ">"
    }
    // if it's a piece of text just dump the text
    else if (type == Node.TEXT) {
        html += node.data
    }

    return html;
}

htmlString(document.documentElement, "") 


  