/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


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


  