/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


//
// return a string representing the html content in html format 
//
function htmlString(node, indent)
{
    var html = ""
    indent += "  "

    var type = node.getNodeType()
    if (type == Node.ELEMENT) {

        // open tag
        html += "\n" + indent + "<" + node.getTagName()

        // dump the attributes if any
        attributes = node.getAttributes()
        if (null != attributes) {
            html += " "
            var countAttrs = attributes.getLength()
            var index = 0
            while(index < countAttrs) {
                att = attributes.item(index)
                if (null != att) {
                    html += att.toString()
                }
                index++
            }
        }

        // end tag
        html += ">"

        // recursively dump the children
        if (node.hasChildNodes()) {
            // get the children
            var children = node.getChildNodes()
            var length = children.getLength()
            var child = children.getNextNode()
            var count = 0;
            while(count < length) {
                html += htmlString(child, indent)
                child = children.getNextNode()
                count++
            }
        }

        // close tag
        html += "\n" + indent + "</" + node.getTagName() + ">"
    }
    // if it's a piece of text just dump the text
    else if (type == Node.TEXT) {
        html += node.data
    }

    return html;
}

htmlString(document.documentElement, "") 


  