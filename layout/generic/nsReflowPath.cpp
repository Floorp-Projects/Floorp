/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   Randell Jesup <rjesup@wgate.com>
 *   Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "pldhash.h"
#include "prenv.h"
#include "nsReflowPath.h"
#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsHTMLReflowCommand.h"

nsReflowPath::~nsReflowPath()
{
    for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i)
        delete NS_STATIC_CAST(nsReflowPath *, mChildren[i]);

    delete mReflowCommand;
}

nsReflowPath::iterator
nsReflowPath::FindChild(nsIFrame *aFrame)
{
    for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i) {
        nsReflowPath *subtree = NS_STATIC_CAST(nsReflowPath *, mChildren[i]);
        if (subtree->mFrame == aFrame)
            return iterator(this, i);
    }

    return iterator(this, -1);
}

nsReflowPath *
nsReflowPath::GetSubtreeFor(nsIFrame *aFrame) const
{
    for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i) {
        nsReflowPath *subtree = NS_STATIC_CAST(nsReflowPath *, mChildren[i]);
        if (subtree->mFrame == aFrame)
            return subtree;
    }

    return nsnull;
}

nsReflowPath *
nsReflowPath::EnsureSubtreeFor(nsIFrame *aFrame)
{
    for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i) {
        nsReflowPath *subtree = NS_STATIC_CAST(nsReflowPath *, mChildren[i]);
        if (subtree->mFrame == aFrame)
            return subtree;
    }

    nsReflowPath *subtree = new nsReflowPath(aFrame);
    mChildren.AppendElement(subtree);

    return subtree;
}

void
nsReflowPath::Remove(iterator &aIterator)
{
    NS_ASSERTION(aIterator.mNode == this, "inconsistent iterator");

    if (aIterator.mIndex >= 0 && aIterator.mIndex < mChildren.Count()) {
        delete NS_STATIC_CAST(nsReflowPath *, mChildren[aIterator.mIndex]);
        mChildren.RemoveElementAt(aIterator.mIndex);
    }
}

#ifdef DEBUG
void
DebugListReflowPath(nsPresContext *aPresContext, nsReflowPath *aReflowPath)
{
    aReflowPath->Dump(aPresContext, stdout, 0);
}

void
nsReflowPath::Dump(nsPresContext *aPresContext, FILE *aFile, int depth)
{
    fprintf(aFile, "%*s nsReflowPath(%p): ", depth, "", this);

    nsFrame::ListTag(aFile, mFrame);

    if (mReflowCommand) {
        fprintf(aFile, " <- ");
        mReflowCommand->List(aFile);
    }

    fprintf(aFile, "\n");

    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
        nsReflowPath *child = NS_STATIC_CAST(nsReflowPath *, mChildren[i]);
        child->Dump(aPresContext, aFile, depth + 1);
    }
}
#endif
