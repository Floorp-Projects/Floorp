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
// travers the html tree and dump out the type of element
//
function traverse(node, indent)
{
    Dump("\n")
    indent += "  "
    var type = node.GetNodeType()

    // if it's an element dump the tag and recurse the children
    if (type == Node.ELEMENT) {

        Dump(indent + node.GetTagName())

        // go through the children
        if (node.HasChildNodes()) {
            var children = node.GetChildNodes()
            var length = children.GetLength()
            var child = children.GetNextNode()
            var count = 0;
            while(count < length) {
                traverse(child, indent)
                child = children.GetNextNode()
                count++
            }
        }
    }
    // it's just text, no tag, dump "Text"
    else if (type == Node.TEXT) {
        Dump(indent + "Text")
    }
}

var node = document.documentElement

traverse(node, "")
Dump("\n")

  