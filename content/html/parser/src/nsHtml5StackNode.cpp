/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2008 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit StackNode.java instead and regenerate.
 */

#define nsHtml5StackNode_cpp__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5Parser.h"
#include "nsHtml5StringLiterals.h"
#include "nsHtml5Atoms.h"

#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5Portability.h"

#include "nsHtml5StackNode.h"


nsHtml5StackNode::nsHtml5StackNode(PRInt32 group, PRInt32 ns, nsIAtom* name, nsIContent* node, PRBool scoping, PRBool special, PRBool fosterParenting, nsIAtom* popName)
  : group(group),
    name(name),
    popName(popName),
    ns(ns),
    node(node),
    scoping(scoping),
    special(special),
    fosterParenting(fosterParenting),
    tainted(PR_FALSE)
{
}


nsHtml5StackNode::nsHtml5StackNode(PRInt32 ns, nsHtml5ElementName* elementName, nsIContent* node)
  : group(elementName->group),
    name(elementName->name),
    popName(elementName->name),
    ns(ns),
    node(node),
    scoping(elementName->scoping),
    special(elementName->special),
    fosterParenting(elementName->fosterParenting),
    tainted(PR_FALSE)
{
}


nsHtml5StackNode::nsHtml5StackNode(PRInt32 ns, nsHtml5ElementName* elementName, nsIContent* node, nsIAtom* popName)
  : group(elementName->group),
    name(elementName->name),
    popName(popName),
    ns(ns),
    node(node),
    scoping(elementName->scoping),
    special(elementName->special),
    fosterParenting(elementName->fosterParenting),
    tainted(PR_FALSE)
{
}


nsHtml5StackNode::nsHtml5StackNode(PRInt32 ns, nsHtml5ElementName* elementName, nsIContent* node, nsIAtom* popName, PRBool scoping)
  : group(elementName->group),
    name(elementName->name),
    popName(popName),
    ns(ns),
    node(node),
    scoping(scoping),
    special(PR_FALSE),
    fosterParenting(PR_FALSE),
    tainted(PR_FALSE)
{
}

void 
nsHtml5StackNode::destructor()
{
}

void
nsHtml5StackNode::initializeStatics()
{
}

void
nsHtml5StackNode::releaseStatics()
{
}


