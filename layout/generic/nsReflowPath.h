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

#ifndef nsReflowPath_h__
#define nsReflowPath_h__

#include "nscore.h"
#include "pldhash.h"
#include "nsReflowType.h"
#include "nsVoidArray.h"

#ifdef DEBUG
#include <stdio.h>
#endif

class nsIFrame;
class nsHTMLReflowCommand;
class nsPresContext;

/**
 * A reflow `path' that is a sparse tree the parallels the frame
 * hierarchy. This is used during an incremental reflow to record the
 * path which the reflow must trace through the frame hierarchy.
 */
class nsReflowPath
{
public:
    /**
     * Construct a reflow path object that parallels the specified
     * frame.
     */
    nsReflowPath(nsIFrame *aFrame)
        : mFrame(aFrame),
          mReflowCommand(nsnull) {}

    ~nsReflowPath();

    /**
     * An iterator for enumerating the reflow path's immediate
     * children.
     */
    class iterator
    {
    protected:
        nsReflowPath *mNode;
        PRInt32 mIndex;

        friend class nsReflowPath;

        iterator(nsReflowPath *aNode, PRInt32 aIndex)
            : mNode(aNode), mIndex(aIndex) {}

        void
        Advance() { --mIndex; }

    public:
        iterator()
            : mNode(nsnull) {}

        iterator(const iterator &iter)
            : mNode(iter.mNode), mIndex(iter.mIndex) {}

        iterator &
        operator=(const iterator &iter) {
            mNode = iter.mNode;
            mIndex = iter.mIndex;
            return *this; }

        nsReflowPath *
        get() const {
            return NS_STATIC_CAST(nsReflowPath *, mNode->mChildren[mIndex]); }

        nsReflowPath *
        get() {
            return NS_STATIC_CAST(nsReflowPath *, mNode->mChildren[mIndex]); }

        nsIFrame *
        operator*() const {
            return get()->mFrame; }

        nsIFrame *&
        operator*() {
            return get()->mFrame; }

        iterator &
        operator++() { Advance(); return *this; }

        iterator
        operator++(int) {
            iterator temp(*this);
            Advance();
            return temp; }

        PRBool
        operator==(const iterator &iter) const {
            return (mNode == iter.mNode) && (mIndex == iter.mIndex); }

        PRBool
        operator!=(const iterator &iter) const {
            return !iter.operator==(*this); }
    };

    /**
     * Return an iterator that points to the first immediate child of
     * the reflow path.
     */
    iterator FirstChild() { return iterator(this, mChildren.Count() - 1); }

    /**
     * Return an iterator that points `one past the end' of the
     * immediate children of the reflow path.
     */
    iterator EndChildren() { return iterator(this, -1); }

    /**
     * Determine if the reflow path contains the specified frame as
     * one of its immediate children.
     */
    PRBool
    HasChild(nsIFrame *aFrame) const {
        return GetSubtreeFor(aFrame) != nsnull; }

    /**
     * Return an iterator over the current reflow path that
     * corresponds to the specified child frame. Returns EndChildren
     * if aFrame is not an immediate child of the reflow path.
     */
    iterator
    FindChild(nsIFrame *aFrame);

    /**
     * Remove the specified child frame from the reflow path, along
     * with any of its descendants. Does nothing if aFrame is not an
     * immediate child of the reflow path.
     */
    void
    RemoveChild(nsIFrame *aFrame) { 
        iterator iter = FindChild(aFrame);
        Remove(iter); }

    /**
     * Return the child reflow path that corresponds to the specified
     * frame, or null if the frame is not an immediate descendant.
     */
    nsReflowPath *
    GetSubtreeFor(nsIFrame *aFrame) const;

    /**
     * Return the child reflow path that corresponds to the specified
     * frame, constructing a new child reflow path if one doesn't
     * exist already.
     */
    nsReflowPath *
    EnsureSubtreeFor(nsIFrame *aFrame);

    /**
     * Remove the child reflow path that corresponds to the specified
     * iterator.
     */
    void
    Remove(iterator &aIterator);

#ifdef DEBUG
    /**
     * Recursively dump the reflow path object and its descendants.
     */
    void
    Dump(nsPresContext *aPresContext, FILE *aFile, int aDepth);
#endif

    /**
     * The frame that this reflow path object is associated with.
     */
    nsIFrame            *mFrame;

    /**
     * If mFrame is the immediate target of an incremental reflow,
     * this contains the reflow command that targeted it. Otherwise,
     * this is null (and mFrame simply lies along the path to a target
     * frame). The reflow path object assumes ownership of the reflow
     * command.
     */
    nsHTMLReflowCommand *mReflowCommand;

protected:
    /**
     * The children of this reflow path; also contains pointers to
     * nsReflowPath objects.
     */
    nsSmallVoidArray     mChildren;

    friend class iterator;
};

#endif
