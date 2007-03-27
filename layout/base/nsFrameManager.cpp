/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* storage of the frame tree and information about it */

#include "nscore.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsCSSFrameConstructor.h"
#include "nsStyleContext.h"
#include "nsStyleChangeList.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "plhash.h"
#include "nsPlaceholderFrame.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#ifdef NS_DEBUG
#include "nsISupportsArray.h"
#include "nsIStyleRule.h"
#endif
#include "nsILayoutHistoryState.h"
#include "nsPresState.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIScrollableFrame.h"

#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIFormControl.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIForm.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "nsLayoutErrors.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"
#include "imgIRequest.h"

#include "nsFrameManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#include "nsIAccessibleEvent.h"
#endif

  #ifdef DEBUG
    //#define NOISY_DEBUG
    //#define DEBUG_UNDISPLAYED_MAP
  #else
    #undef NOISY_DEBUG
    #undef DEBUG_UNDISPLAYED_MAP
  #endif

  #ifdef NOISY_DEBUG
    #define NOISY_TRACE(_msg) \
      printf("%s",_msg);
    #define NOISY_TRACE_FRAME(_msg,_frame) \
      printf("%s ",_msg); nsFrame::ListTag(stdout,_frame); printf("\n");
  #else
    #define NOISY_TRACE(_msg);
    #define NOISY_TRACE_FRAME(_msg,_frame);
  #endif

// IID's

//----------------------------------------------------------------------

struct PlaceholderMapEntry : public PLDHashEntryHdr {
  // key (the out of flow frame) can be obtained through placeholder frame
  nsPlaceholderFrame *placeholderFrame;
};

PR_STATIC_CALLBACK(PRBool)
PlaceholderMapMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const PlaceholderMapEntry *entry =
    NS_STATIC_CAST(const PlaceholderMapEntry*, hdr);
  NS_ASSERTION(entry->placeholderFrame->GetOutOfFlowFrame() !=
               (void*)0xdddddddd,
               "Dead placeholder in placeholder map");
  return entry->placeholderFrame->GetOutOfFlowFrame() == key;
}

static PLDHashTableOps PlaceholderMapOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  PlaceholderMapMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

//----------------------------------------------------------------------

struct PrimaryFrameMapEntry : public PLDHashEntryHdr {
  // key (the content node) can almost always be obtained through the
  // frame.  If it weren't for the way image maps (mis)used the primary
  // frame map, we'd be able to have a 2 word entry instead of a 3 word
  // entry.
  nsIContent *content;
  nsIFrame *frame;
};

  // These ops should be used if/when we switch back to a 2-word entry.
  // See comment in |PrimaryFrameMapEntry| above.
#if 0
PR_STATIC_CALLBACK(PRBool)
PrimaryFrameMapMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const PrimaryFrameMapEntry *entry =
    NS_STATIC_CAST(const PrimaryFrameMapEntry*, hdr);
  return entry->frame->GetContent() == key;
}

static PLDHashTableOps PrimaryFrameMapOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  PrimaryFrameMapMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};
#endif /* 0 */

//----------------------------------------------------------------------

// XXXldb This seems too complicated for what I think it's doing, and it
// should also be using pldhash rather than plhash to use less memory.

class UndisplayedNode {
public:
  UndisplayedNode(nsIContent* aContent, nsStyleContext* aStyle)
    : mContent(aContent),
      mStyle(aStyle),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(UndisplayedNode);
  }

  NS_HIDDEN ~UndisplayedNode()
  {
    MOZ_COUNT_DTOR(UndisplayedNode);
    delete mNext;
  }

  nsCOMPtr<nsIContent>      mContent;
  nsRefPtr<nsStyleContext>  mStyle;
  UndisplayedNode*          mNext;
};

class nsFrameManagerBase::UndisplayedMap {
public:
  UndisplayedMap(PRUint32 aNumBuckets = 16) NS_HIDDEN;
  ~UndisplayedMap(void) NS_HIDDEN;

  NS_HIDDEN_(UndisplayedNode*) GetFirstNode(nsIContent* aParentContent);

  NS_HIDDEN_(nsresult) AddNodeFor(nsIContent* aParentContent,
                                  nsIContent* aChild, nsStyleContext* aStyle);

  NS_HIDDEN_(void) RemoveNodeFor(nsIContent* aParentContent,
                                 UndisplayedNode* aNode);

  NS_HIDDEN_(void) RemoveNodesFor(nsIContent* aParentContent);

  // Removes all entries from the hash table
  NS_HIDDEN_(void)  Clear(void);

protected:
  NS_HIDDEN_(PLHashEntry**) GetEntryFor(nsIContent* aParentContent);
  NS_HIDDEN_(void)          AppendNodeFor(UndisplayedNode* aNode,
                                          nsIContent* aParentContent);

  PLHashTable*  mTable;
  PLHashEntry** mLastLookup;
};

//----------------------------------------------------------------------

nsFrameManager::nsFrameManager()
{
}

nsFrameManager::~nsFrameManager()
{
  NS_ASSERTION(!mPresShell, "nsFrameManager::Destroy never called");
}

nsresult
nsFrameManager::Init(nsIPresShell* aPresShell,
                     nsStyleSet*  aStyleSet)
{
  if (!aPresShell) {
    NS_ERROR("null pres shell");
    return NS_ERROR_FAILURE;
  }

  if (!aStyleSet) {
    NS_ERROR("null style set");
    return NS_ERROR_FAILURE;
  }

  mPresShell = aPresShell;
  mStyleSet = aStyleSet;
  return NS_OK;
}

void
nsFrameManager::Destroy()
{
  NS_ASSERTION(mPresShell, "Frame manager already shut down.");

  // Destroy the frame hierarchy.
  mPresShell->SetIgnoreFrameDestruction(PR_TRUE);

  mIsDestroyingFrames = PR_TRUE;  // This flag prevents GetPrimaryFrameFor from returning pointers to destroyed frames

  // Unregister all placeholders before tearing down the frame tree
  nsFrameManager::ClearPlaceholderFrameMap();

  if (mRootFrame) {
    mRootFrame->Destroy();
    mRootFrame = nsnull;
  }
  
  nsFrameManager::ClearPrimaryFrameMap();
  delete mUndisplayedMap;
  mUndisplayedMap = nsnull;

  mPresShell = nsnull;
}

nsIFrame*
nsFrameManager::GetCanvasFrame()
{
  if (mRootFrame) {
    // walk the children of the root frame looking for a frame with type==canvas
    // start at the root
    nsIFrame* childFrame = mRootFrame;
    while (childFrame) {
      // get each sibling of the child and check them, startig at the child
      nsIFrame *siblingFrame = childFrame;
      while (siblingFrame) {
        if (siblingFrame->GetType() == nsGkAtoms::canvasFrame) {
          // this is it
          return siblingFrame;
        } else {
          siblingFrame = siblingFrame->GetNextSibling();
        }
      }
      // move on to the child's child
      childFrame = childFrame->GetFirstChild(nsnull);
    }
  }
  return nsnull;
}

//----------------------------------------------------------------------

// Primary frame functions
nsIFrame*
nsFrameManager::GetPrimaryFrameFor(nsIContent* aContent,
                                   PRInt32 aIndexHint)
{
  NS_ENSURE_TRUE(aContent, nsnull);

  if (mIsDestroyingFrames) {
#ifdef DEBUG
    printf("GetPrimaryFrameFor() called while nsFrameManager is being destroyed!\n");
#endif
    return nsnull;
  }

  if (!aContent->MayHaveFrame()) {
    return nsnull;
  }

  if (mPrimaryFrameMap.ops) {
    PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
        PL_DHashTableOperate(&mPrimaryFrameMap, aContent, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->frame;
    }

    // XXX: todo:  Add a lookup into the undisplay map to skip searches 
    //             if we already know the content has no frame.
    //             nsCSSFrameConstructor calls SetUndisplayedContent() for every
    //             content node that has display: none.
    //             Today, the undisplay map doesn't quite support what we need.
    //             We need to see if we can add a method to make a search for aContent 
    //             very fast in the embedded hash table.
    //             This would almost completely remove the lookup penalty for things
    //             like <SCRIPT> and comments in very large documents.
    // XXX with the nsIContent::MayHaveFrame bit, is that really necessary now?

    // Give the frame construction code the opportunity to return the
    // frame that maps the content object

    // if the prev sibling of aContent has a cached primary frame,
    // pass that data in to the style set to speed things up
    // if any methods in here fail, don't report that failure
    // we're just trying to enhance performance here, not test for correctness
    nsFindFrameHint hint;
    nsIContent* parent = aContent->GetParent();
    if (parent)
    {
      PRInt32 index = aIndexHint >= 0 ? aIndexHint : parent->IndexOf(aContent);
      if (index > 0)  // no use looking if it's the first child
      {
        nsIContent *prevSibling;
        do {
          prevSibling = parent->GetChildAt(--index);
        } while (index &&
                 (prevSibling->IsNodeOfType(nsINode::eTEXT) ||
                  prevSibling->IsNodeOfType(nsINode::eCOMMENT) ||
                  prevSibling->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)));
        if (prevSibling) {
          entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
                          PL_DHashTableOperate(&mPrimaryFrameMap, prevSibling,
                                               PL_DHASH_LOOKUP));
          if (PL_DHASH_ENTRY_IS_BUSY(entry))
            hint.mPrimaryFrameForPrevSibling = entry->frame;
        }
      }
    }

    // walk the frame tree to find the frame that maps aContent.  
    // Use the hint if we have it.
    nsIFrame *result;

    mPresShell->FrameConstructor()->
      FindPrimaryFrameFor(this, aContent, &result, 
                          hint.mPrimaryFrameForPrevSibling ? &hint : nsnull);

    return result;
  }

  return nsnull;
}

nsresult
nsFrameManager::SetPrimaryFrameFor(nsIContent* aContent,
                                   nsIFrame*   aPrimaryFrame)
{
  NS_ENSURE_ARG_POINTER(aContent);
  NS_ASSERTION(aPrimaryFrame && aPrimaryFrame->GetParent(),
               "BOGUS!");

  // This code should be used if/when we switch back to a 2-word entry
  // in the primary frame map.
#if 0
  NS_PRECONDITION(aPrimaryFrame->GetContent() == aContent, "wrong content");
#endif

  // Create a new hashtable if necessary
  if (!mPrimaryFrameMap.ops) {
    if (!PL_DHashTableInit(&mPrimaryFrameMap, PL_DHashGetStubOps(), nsnull,
                           sizeof(PrimaryFrameMapEntry), 16)) {
      mPrimaryFrameMap.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Add a mapping to the hash table
  PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
      PL_DHashTableOperate(&mPrimaryFrameMap, aContent, PL_DHASH_ADD));
#ifdef DEBUG_dbaron
  if (entry->frame) {
    NS_WARNING("already have primary frame for content");
  }
#endif
  entry->frame = aPrimaryFrame;
  entry->content = aContent;
    
  return NS_OK;
}

void
nsFrameManager::RemoveAsPrimaryFrame(nsIContent* aContent,
                                     nsIFrame* aPrimaryFrame)
{
  NS_PRECONDITION(aPrimaryFrame, "Must have a frame");
  if (aContent && mPrimaryFrameMap.ops) {
    PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
        PL_DHashTableOperate(&mPrimaryFrameMap, aContent, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->frame == aPrimaryFrame) {
      // Don't use PL_DHashTableRawRemove, since we want the table to
      // shrink as needed.
      PL_DHashTableOperate(&mPrimaryFrameMap, aContent, PL_DHASH_REMOVE);
    }
  }

  aPrimaryFrame->RemovedAsPrimaryFrame();
}

void
nsFrameManager::ClearPrimaryFrameMap()
{
  if (mPrimaryFrameMap.ops) {
    PL_DHashTableFinish(&mPrimaryFrameMap);
    mPrimaryFrameMap.ops = nsnull;
  }
}

// Placeholder frame functions
nsPlaceholderFrame*
nsFrameManager::GetPlaceholderFrameFor(nsIFrame*  aFrame)
{
  NS_PRECONDITION(aFrame, "null param unexpected");

  if (mPlaceholderMap.ops) {
    PlaceholderMapEntry *entry = NS_STATIC_CAST(PlaceholderMapEntry*,
           PL_DHashTableOperate(NS_CONST_CAST(PLDHashTable*, &mPlaceholderMap),
                                aFrame, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->placeholderFrame;
    }
  }

  return nsnull;
}

nsresult
nsFrameManager::RegisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame)
{
  NS_PRECONDITION(aPlaceholderFrame, "null param unexpected");
  NS_PRECONDITION(nsGkAtoms::placeholderFrame == aPlaceholderFrame->GetType(),
                  "unexpected frame type");
  if (!mPlaceholderMap.ops) {
    if (!PL_DHashTableInit(&mPlaceholderMap, &PlaceholderMapOps, nsnull,
                           sizeof(PlaceholderMapEntry), 16)) {
      mPlaceholderMap.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  PlaceholderMapEntry *entry = NS_STATIC_CAST(PlaceholderMapEntry*, 
         PL_DHashTableOperate(&mPlaceholderMap,
                              aPlaceholderFrame->GetOutOfFlowFrame(),
                              PL_DHASH_ADD));
  if (!entry)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ASSERTION(!entry->placeholderFrame, "Registering a placeholder for a frame that already has a placeholder!");
  entry->placeholderFrame = aPlaceholderFrame;

  return NS_OK;
}

void
nsFrameManager::UnregisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame)
{
  NS_PRECONDITION(aPlaceholderFrame, "null param unexpected");
  NS_PRECONDITION(nsGkAtoms::placeholderFrame == aPlaceholderFrame->GetType(),
                  "unexpected frame type");

  if (mPlaceholderMap.ops) {
    PL_DHashTableOperate(&mPlaceholderMap,
                         aPlaceholderFrame->GetOutOfFlowFrame(),
                         PL_DHASH_REMOVE);
  }
}

PR_STATIC_CALLBACK(PLDHashOperator)
UnregisterPlaceholders(PLDHashTable* table, PLDHashEntryHdr* hdr,
                       PRUint32 number, void* arg)
{
  PlaceholderMapEntry* entry = NS_STATIC_CAST(PlaceholderMapEntry*, hdr);
  entry->placeholderFrame->SetOutOfFlowFrame(nsnull);
  return PL_DHASH_NEXT;
}

void
nsFrameManager::ClearPlaceholderFrameMap()
{
  if (mPlaceholderMap.ops) {
    PL_DHashTableEnumerate(&mPlaceholderMap, UnregisterPlaceholders, nsnull);
    PL_DHashTableFinish(&mPlaceholderMap);
    mPlaceholderMap.ops = nsnull;
  }
}

//----------------------------------------------------------------------

nsStyleContext*
nsFrameManager::GetUndisplayedContent(nsIContent* aContent)
{
  if (!aContent || !mUndisplayedMap)
    return nsnull;

  nsIContent* parent = aContent->GetParent();
  if (!parent)
    return nsnull;

  for (UndisplayedNode* node = mUndisplayedMap->GetFirstNode(parent);
         node; node = node->mNext) {
    if (node->mContent == aContent)
      return node->mStyle;
  }

  return nsnull;
}
  
void
nsFrameManager::SetUndisplayedContent(nsIContent* aContent, 
                                      nsStyleContext* aStyleContext)
{
#ifdef DEBUG_UNDISPLAYED_MAP
  static int i = 0;
  printf("SetUndisplayedContent(%d): p=%p \n", i++, (void *)aContent);
#endif

  NS_ASSERTION(!GetUndisplayedContent(aContent),
               "Already have an undisplayed context entry for aContent");

  if (! mUndisplayedMap) {
    mUndisplayedMap = new UndisplayedMap;
  }
  if (mUndisplayedMap) {
    nsIContent* parent = aContent->GetParent();
    NS_ASSERTION(parent, "undisplayed content must have a parent");
    if (parent) {
      mUndisplayedMap->AddNodeFor(parent, aContent, aStyleContext);
    }
  }
}

void
nsFrameManager::ChangeUndisplayedContent(nsIContent* aContent, 
                                         nsStyleContext* aStyleContext)
{
  NS_ASSERTION(mUndisplayedMap, "no existing undisplayed content");
  
#ifdef DEBUG_UNDISPLAYED_MAP
   static int i = 0;
   printf("ChangeUndisplayedContent(%d): p=%p \n", i++, (void *)aContent);
#endif

  for (UndisplayedNode* node = mUndisplayedMap->GetFirstNode(aContent->GetParent());
         node; node = node->mNext) {
    if (node->mContent == aContent) {
      node->mStyle = aStyleContext;
      return;
    }
  }

  NS_NOTREACHED("no existing undisplayed content");
}

void
nsFrameManager::ClearUndisplayedContentIn(nsIContent* aContent,
                                          nsIContent* aParentContent)
{
#ifdef DEBUG_UNDISPLAYED_MAP
  static int i = 0;
  printf("ClearUndisplayedContent(%d): content=%p parent=%p --> ", i++, (void *)aContent, (void*)aParentContent);
#endif
  
  if (mUndisplayedMap) {
    UndisplayedNode* node = mUndisplayedMap->GetFirstNode(aParentContent);
    while (node) {
      if (node->mContent == aContent) {
        mUndisplayedMap->RemoveNodeFor(aParentContent, node);

#ifdef DEBUG_UNDISPLAYED_MAP
        printf( "REMOVED!\n");
#endif
#ifdef DEBUG
        // make sure that there are no more entries for the same content
        nsStyleContext *context = GetUndisplayedContent(aContent);
        NS_ASSERTION(context == nsnull, "Found more undisplayed content data after removal");
#endif
        return;
      }
      node = node->mNext;
    }
  }
}

void
nsFrameManager::ClearAllUndisplayedContentIn(nsIContent* aParentContent)
{
#ifdef DEBUG_UNDISPLAYED_MAP
  static int i = 0;
  printf("ClearAllUndisplayedContentIn(%d): parent=%p \n", i++, (void*)aParentContent);
#endif

  if (mUndisplayedMap) {
    mUndisplayedMap->RemoveNodesFor(aParentContent);
  }
}

void
nsFrameManager::ClearUndisplayedContentMap()
{
#ifdef DEBUG_UNDISPLAYED_MAP
  static int i = 0;
  printf("ClearUndisplayedContentMap(%d)\n", i++);
#endif

  if (mUndisplayedMap) {
    mUndisplayedMap->Clear();
  }
}

//----------------------------------------------------------------------

nsresult
nsFrameManager::InsertFrames(nsIFrame*       aParentFrame,
                             nsIAtom*        aListName,
                             nsIFrame*       aPrevFrame,
                             nsIFrame*       aFrameList)
{
  NS_PRECONDITION(!aPrevFrame || !aPrevFrame->GetNextContinuation(),
                  "aPrevFrame must be the last continuation in its chain!");

  return aParentFrame->InsertFrames(aListName, aPrevFrame, aFrameList);
}

nsresult
nsFrameManager::RemoveFrame(nsIFrame*       aParentFrame,
                            nsIAtom*        aListName,
                            nsIFrame*       aOldFrame)
{
  // In case the reflow doesn't invalidate anything since it just leaves
  // a gap where the old frame was, we invalidate it here.  (This is
  // reasonably likely to happen when removing a last child in a way
  // that doesn't change the size of the parent.)
  // This has to sure to invalidate the entire overflow rect; this
  // is important in the presence of absolute positioning
  aOldFrame->Invalidate(aOldFrame->GetOverflowRect());

  return aParentFrame->RemoveFrame(aListName, aOldFrame);
}

//----------------------------------------------------------------------

void
nsFrameManager::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // We've already removed from the primary frame map once, but we're
  // going to try to do it again here to fix callers of GetPrimaryFrameFor
  // during frame destruction, since this problem keeps coming back to
  // bite us.  We may want to remove the previous caller.
  if (mPrimaryFrameMap.ops) {
    PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
        PL_DHashTableOperate(&mPrimaryFrameMap, aFrame->GetContent(), PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->frame == aFrame) {
      NS_NOTREACHED("frame was not removed from primary frame map before "
                    "destruction or was readded to map after being removed");
      PL_DHashTableRawRemove(&mPrimaryFrameMap, entry);
    }
  }
}

#ifdef NS_DEBUG
static void
DumpContext(nsIFrame* aFrame, nsStyleContext* aContext)
{
  if (aFrame) {
    fputs("frame: ", stdout);
    nsAutoString  name;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
      fputs(NS_LossyConvertUTF16toASCII(name).get(), stdout);
    }
    fprintf(stdout, " (%p)", NS_STATIC_CAST(void*, aFrame));
  }
  if (aContext) {
    fprintf(stdout, " style: %p ", NS_STATIC_CAST(void*, aContext));

    nsIAtom* pseudoTag = aContext->GetPseudoType();
    if (pseudoTag) {
      nsAutoString  buffer;
      pseudoTag->ToString(buffer);
      fputs(NS_LossyConvertUTF16toASCII(buffer).get(), stdout);
      fputs(" ", stdout);
    }

/* XXXdwh fix debugging here.  Need to add a List method to nsRuleNode
   and have the context call list on its rule node.
    PRInt32 count = aContext->GetStyleRuleCount();
    if (0 < count) {
      fputs("{\n", stdout);
      nsISupportsArray* rules = aContext->GetStyleRules();
      PRInt32 ix;
      for (ix = 0; ix < count; ix++) {
        nsIStyleRule* rule = (nsIStyleRule*)rules->ElementAt(ix);
        rule->List(stdout, 1);
        NS_RELEASE(rule);
      }
      NS_RELEASE(rules);
      fputs("}\n", stdout);
    }
    else 
    */
    {
      fputs("{}\n", stdout);
    }
  }
}

static void
VerifySameTree(nsStyleContext* aContext1, nsStyleContext* aContext2)
{
  nsStyleContext* top1 = aContext1;
  nsStyleContext* top2 = aContext2;
  nsStyleContext* parent;
  for (;;) {
    parent = top1->GetParent();
    if (!parent)
      break;
    top1 = parent;
  }
  for (;;) {
    parent = top2->GetParent();
    if (!parent)
      break;
    top2 = parent;
  }
  if (top1 != top2)
    printf("Style contexts are not in the same style context tree.\n");
}

static void
VerifyContextParent(nsPresContext* aPresContext, nsIFrame* aFrame, 
                    nsStyleContext* aContext, nsStyleContext* aParentContext)
{
  // get the contexts not provided
  if (!aContext) {
    aContext = aFrame->GetStyleContext();
  }

  if (!aParentContext) {
    // Get the correct parent context from the frame
    //  - if the frame is a placeholder, we get the out of flow frame's context 
    //    as the parent context instead of asking the frame

    // get the parent context from the frame (indirectly)
    nsIFrame* providerFrame = nsnull;
    PRBool providerIsChild;
    aFrame->GetParentStyleContextFrame(aPresContext,
                                       &providerFrame, &providerIsChild);
    if (providerFrame)
      aParentContext = providerFrame->GetStyleContext();
    // aParentContext could still be null
  }

  NS_ASSERTION(aContext, "Failure to get required contexts");
  nsStyleContext* actualParentContext = aContext->GetParent();

  if (aParentContext) {
    if (aParentContext != actualParentContext) {
      DumpContext(aFrame, aContext);
      if (aContext == aParentContext) {
        fputs("Using parent's style context\n\n", stdout);
      }
      else {
        fputs("Wrong parent style context: ", stdout);
        DumpContext(nsnull, actualParentContext);
        fputs("should be using: ", stdout);
        DumpContext(nsnull, aParentContext);
        VerifySameTree(actualParentContext, aParentContext);
        fputs("\n", stdout);
      }
    }
  }
  else {
    if (actualParentContext) {
      DumpContext(aFrame, aContext);
      fputs("Has parent context: ", stdout);
      DumpContext(nsnull, actualParentContext);
      fputs("Should be null\n\n", stdout);
    }
  }
}

static void
VerifyStyleTree(nsPresContext* aPresContext, nsIFrame* aFrame,
                nsStyleContext* aParentContext)
{
  nsStyleContext*  context = aFrame->GetStyleContext();
  VerifyContextParent(aPresContext, aFrame, context, nsnull);

  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;
  nsIFrame* child;

  do {
    child = aFrame->GetFirstChild(childList);
    while (child) {
      if (NS_FRAME_OUT_OF_FLOW != (child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        if (nsGkAtoms::placeholderFrame == child->GetType()) { 
          // placeholder: first recurse and verify the out of flow frame,
          // then verify the placeholder's context
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);

          // recurse to out of flow frame, letting the parent context get resolved
          VerifyStyleTree(aPresContext, outOfFlowFrame, nsnull);

          // verify placeholder using the parent frame's context as
          // parent context
          VerifyContextParent(aPresContext, child, nsnull, nsnull);
        }
        else { // regular frame
          VerifyStyleTree(aPresContext, child, nsnull);
        }
      }
      child = child->GetNextSibling();
    }

    childList = aFrame->GetAdditionalChildListName(listIndex++);
  } while (childList);
  
  // do additional contexts 
  PRInt32 contextIndex = -1;
  while (1) {
    nsStyleContext* extraContext = aFrame->GetAdditionalStyleContext(++contextIndex);
    if (extraContext) {
      VerifyContextParent(aPresContext, aFrame, extraContext, context);
    }
    else {
      break;
    }
  }
}

void
nsFrameManager::DebugVerifyStyleTree(nsIFrame* aFrame)
{
  if (aFrame) {
    nsStyleContext* context = aFrame->GetStyleContext();
    nsStyleContext* parentContext = context->GetParent();
    VerifyStyleTree(GetPresContext(), aFrame, parentContext);
  }
}

#endif // DEBUG

nsresult
nsFrameManager::ReParentStyleContext(nsIFrame* aFrame)
{
  // DO NOT verify the style tree before reparenting.  The frame
  // tree has already been changed, so this check would just fail.
  nsStyleContext* oldContext = aFrame->GetStyleContext();
  // XXXbz can oldContext really ever be null?
  if (oldContext) {
    nsPresContext *presContext = GetPresContext();
    nsRefPtr<nsStyleContext> newContext;
    nsIFrame* providerFrame = nsnull;
    PRBool providerIsChild = PR_FALSE;
    nsIFrame* providerChild = nsnull;
    aFrame->GetParentStyleContextFrame(presContext, &providerFrame,
                                       &providerIsChild);
    nsStyleContext* newParentContext = nsnull;
    if (providerIsChild) {
      ReParentStyleContext(providerFrame);
      newParentContext = providerFrame->GetStyleContext();
      providerChild = providerFrame;
    } else if (providerFrame) {
      newParentContext = providerFrame->GetStyleContext();
    } else {
      NS_NOTREACHED("Reparenting something that has no usable parent? "
                    "Shouldn't happen!");
    }
    // XXX need to do something here to produce the correct style context
    // for an IB split whose first inline part is inside a first-line frame.
    // Currently the IB anonymous block's style context takes the first part's
    // style context as parent, which is wrong since first-line style should
    // not apply to the anonymous block.

    newContext = mStyleSet->ReParentStyleContext(presContext, oldContext,
                                                 newParentContext);
    if (newContext) {
      if (newContext != oldContext) {
        PRInt32 listIndex = 0;
        nsIAtom* childList = nsnull;
        nsIFrame* child;
          
        aFrame->SetStyleContext(newContext);

        do {
          child = aFrame->GetFirstChild(childList);
          while (child) {
            // only do frames that are in flow
            if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
              if (nsGkAtoms::placeholderFrame == child->GetType()) {
                // get out of flow frame and recurse there
                nsIFrame* outOfFlowFrame =
                  nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
                NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

                NS_ASSERTION(outOfFlowFrame != providerChild,
                             "Out of flow provider?");

                ReParentStyleContext(outOfFlowFrame);

                // reparent placeholder too
                ReParentStyleContext(child);
              }
              else if (child != providerChild) {
                // regular frame, not reparented yet
                ReParentStyleContext(child);
              }
            }

            child = child->GetNextSibling();
          }

          childList = aFrame->GetAdditionalChildListName(listIndex++);
        } while (childList);

        // If this frame is part of an IB split, then the style context of
        // the next part of the split might be a child of our style context.
        // Reparent its style context just in case one of our ancestors
        // (split or not) hasn't done so already). It's not a problem to
        // reparent the same frame twice because the "if (newContext !=
        // oldContext)" check will prevent us from redoing work.
        if ((aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) &&
            !aFrame->GetPrevInFlow()) {
          nsIFrame* sib = NS_STATIC_CAST(nsIFrame*, aFrame->GetProperty(nsGkAtoms::IBSplitSpecialSibling));
          if (sib) {
            ReParentStyleContext(sib);
          }
        }

        // do additional contexts 
        PRInt32 contextIndex = -1;
        while (1) {
          nsStyleContext* oldExtraContext =
            aFrame->GetAdditionalStyleContext(++contextIndex);
          if (oldExtraContext) {
            nsRefPtr<nsStyleContext> newExtraContext;
            newExtraContext = mStyleSet->ReParentStyleContext(presContext,
                                                              oldExtraContext,
                                                              newContext);
            if (newExtraContext) {
              aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
            }
          }
          else {
            break;
          }
        }
#ifdef DEBUG
        VerifyStyleTree(GetPresContext(), aFrame, newParentContext);
#endif
      }
    }
  }
  return NS_OK;
}

static nsChangeHint
CaptureChange(nsStyleContext* aOldContext, nsStyleContext* aNewContext,
              nsIFrame* aFrame, nsIContent* aContent,
              nsStyleChangeList* aChangeList, nsChangeHint aMinChange,
              nsChangeHint aChangeToAssume)
{
  nsChangeHint ourChange = aOldContext->CalcStyleDifference(aNewContext);
  NS_UpdateHint(ourChange, aChangeToAssume);
  if (NS_UpdateHint(aMinChange, ourChange)) {
    aChangeList->AppendChange(aFrame, aContent, ourChange);
  }
  return aMinChange;
}

nsChangeHint
nsFrameManager::ReResolveStyleContext(nsPresContext    *aPresContext,
                                      nsIFrame          *aFrame,
                                      nsIContent        *aParentContent,
                                      nsStyleChangeList *aChangeList, 
                                      nsChangeHint       aMinChange)
{
  // XXXldb get new context from prev-in-flow if possible, to avoid
  // duplication.  (Or should we just let |GetContext| handle that?)
  // Getting the hint would be nice too, but that's harder.

  // XXXbryner we may be able to avoid some of the refcounting goop here.
  // We do need a reference to oldContext for the lifetime of this function, and it's possible
  // that the frame has the last reference to it, so AddRef it here.

  nsChangeHint assumeDifferenceHint = NS_STYLE_HINT_NONE;
  nsStyleContext* oldContext = aFrame->GetStyleContext();
  nsStyleSet* styleSet = aPresContext->StyleSet();
#ifdef ACCESSIBILITY
  PRBool isAccessibilityActive = mPresShell->IsAccessibilityActive();
  PRBool isVisible;
  if (isAccessibilityActive) {
    isVisible = aFrame->GetStyleVisibility()->IsVisible();
  }
#endif

  if (oldContext) {
    oldContext->AddRef();
    nsIAtom* const pseudoTag = oldContext->GetPseudoType();
    nsIContent* localContent = aFrame->GetContent();
    nsIContent* content = localContent ? localContent : aParentContent;

    nsStyleContext* parentContext;
    nsIFrame* resolvedChild = nsnull;
    // Get the frame providing the parent style context.  If it is a
    // child, then resolve the provider first.
    nsIFrame* providerFrame = nsnull;
    PRBool providerIsChild = PR_FALSE;
    aFrame->GetParentStyleContextFrame(aPresContext,
                                       &providerFrame, &providerIsChild); 
    if (!providerIsChild) {
      if (providerFrame)
        parentContext = providerFrame->GetStyleContext();
      else
        parentContext = nsnull;
    }
    else {
      // resolve the provider here (before aFrame below).

      // assumeDifferenceHint forces the parent's change to be also
      // applied to this frame, no matter what
      // nsStyleStruct::CalcStyleDifference says. CalcStyleDifference
      // can't be trusted because it assumes any changes to the parent
      // style context provider will be automatically propagated to
      // the frame(s) with child style contexts.
      assumeDifferenceHint = ReResolveStyleContext(aPresContext, providerFrame,
                                                   content, aChangeList, aMinChange);

      // The provider's new context becomes the parent context of
      // aFrame's context.
      parentContext = providerFrame->GetStyleContext();
      // Set |resolvedChild| so we don't bother resolving the
      // provider again.
      resolvedChild = providerFrame;
    }
    
    // do primary context
    nsStyleContext* newContext = nsnull;
    if (pseudoTag == nsCSSAnonBoxes::mozNonElement) {
      NS_ASSERTION(localContent,
                   "non pseudo-element frame without content node");
      newContext = styleSet->ResolveStyleForNonElement(parentContext).get();
    }
    else if (pseudoTag) {
      nsIContent* pseudoContent =
          aParentContent ? aParentContent : localContent;
      if (pseudoTag == nsCSSPseudoElements::before ||
          pseudoTag == nsCSSPseudoElements::after) {
        // XXX what other pseudos do we need to treat like this?
        newContext = styleSet->ProbePseudoStyleFor(pseudoContent,
                                                   pseudoTag,
                                                   parentContext).get();
        if (!newContext) {
          // This pseudo should no longer exist; gotta reframe
          NS_UpdateHint(aMinChange, nsChangeHint_ReconstructFrame);
          aChangeList->AppendChange(aFrame, pseudoContent,
                                    nsChangeHint_ReconstructFrame);
          // We're reframing anyway; just keep the same context
          newContext = oldContext;
          newContext->AddRef();
        }
      } else {
        newContext = styleSet->ResolvePseudoStyleFor(pseudoContent,
                                                     pseudoTag,
                                                     parentContext).get();
      }
    }
    else {
      NS_ASSERTION(localContent,
                   "non pseudo-element frame without content node");
      newContext = styleSet->ResolveStyleFor(content, parentContext).get();
    }
    NS_ASSERTION(newContext, "failed to get new style context");
    if (newContext) {
      if (!parentContext) {
        if (oldContext->GetRuleNode() == newContext->GetRuleNode()) {
          // We're the root of the style context tree and the new style
          // context returned has the same rule node.  This means that
          // we can use FindChildWithRules to keep a lot of the old
          // style contexts around.  However, we need to start from the
          // same root.
          newContext->Release();
          newContext = oldContext;
          newContext->AddRef();
        }
      }

      if (newContext != oldContext) {
        aMinChange = CaptureChange(oldContext, newContext, aFrame,
                                   content, aChangeList, aMinChange,
                                   assumeDifferenceHint);
        if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
          // if frame gets regenerated, let it keep old context
          aFrame->SetStyleContext(newContext);
        }
        // if old context had image and new context does not have the same image, 
        // stop the image load for the frame
        const nsStyleBackground* oldColor = oldContext->GetStyleBackground();
        const nsStyleBackground* newColor = newContext->GetStyleBackground();

        if (oldColor->mBackgroundImage) {
          PRBool stopImages = !newColor->mBackgroundImage;
          if (!stopImages) {
            nsCOMPtr<nsIURI> oldURI, newURI;
            oldColor->mBackgroundImage->GetURI(getter_AddRefs(oldURI));
            newColor->mBackgroundImage->GetURI(getter_AddRefs(newURI));
            PRBool equal;
            stopImages =
              NS_FAILED(oldURI->Equals(newURI, &equal)) || !equal;
          }
          if (stopImages) {
            // stop the image loading for the frame, the image has changed
            aPresContext->StopImagesFor(aFrame);
          }
        }
      }
      oldContext->Release();
    }
    else {
      NS_ERROR("resolve style context failed");
      newContext = oldContext;  // new context failed, recover... (take ref)
      oldContext = nsnull;
    }

    // do additional contexts 
    PRInt32 contextIndex = -1;
    while (1 == 1) {
      nsStyleContext* oldExtraContext = nsnull;
      oldExtraContext = aFrame->GetAdditionalStyleContext(++contextIndex);
      if (oldExtraContext) {
        nsStyleContext* newExtraContext = nsnull;
        nsIAtom* const extraPseudoTag = oldExtraContext->GetPseudoType();
        NS_ASSERTION(extraPseudoTag &&
                     extraPseudoTag != nsCSSAnonBoxes::mozNonElement,
                     "extra style context is not pseudo element");
        newExtraContext = styleSet->ResolvePseudoStyleFor(content,
                                                          extraPseudoTag,
                                                          newContext).get();
        if (newExtraContext) {
          if (oldExtraContext != newExtraContext) {
            aMinChange = CaptureChange(oldExtraContext, newExtraContext,
                                       aFrame, content, aChangeList,
                                       aMinChange, assumeDifferenceHint);
            if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
              aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
            }
          }
          newExtraContext->Release();
        }
      }
      else {
        break;
      }
    }

    // now look for undisplayed child content and pseudos
    if (localContent && mUndisplayedMap) {
      for (UndisplayedNode* undisplayed =
                                   mUndisplayedMap->GetFirstNode(localContent);
           undisplayed; undisplayed = undisplayed->mNext) {
        nsRefPtr<nsStyleContext> undisplayedContext;
        nsIAtom* const undisplayedPseudoTag = undisplayed->mStyle->GetPseudoType();
        if (!undisplayedPseudoTag) {  // child content
          undisplayedContext = styleSet->ResolveStyleFor(undisplayed->mContent,
                                                         newContext);
        }
        else if (undisplayedPseudoTag == nsCSSAnonBoxes::mozNonElement) {
          undisplayedContext = styleSet->ResolveStyleForNonElement(newContext);
        }
        else {  // pseudo element
          NS_NOTREACHED("no pseudo elements in undisplayed map");
          NS_ASSERTION(undisplayedPseudoTag, "pseudo element without tag");
          undisplayedContext = styleSet->ResolvePseudoStyleFor(localContent,
                                                               undisplayedPseudoTag,
                                                               newContext);
        }
        if (undisplayedContext) {
          const nsStyleDisplay* display = undisplayedContext->GetStyleDisplay();
          if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
            aChangeList->AppendChange(nsnull,
                                      undisplayed->mContent
                                      ? NS_STATIC_CAST(nsIContent*,
                                                       undisplayed->mContent)
                                      : localContent, 
                                      NS_STYLE_HINT_FRAMECHANGE);
            // The node should be removed from the undisplayed map when
            // we reframe it.
          } else {
            // update the undisplayed node with the new context
            undisplayed->mStyle = undisplayedContext;
          }
        }
      }
    }

    if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
      // Make sure not to do this for pseudo-frames -- those can't have :before
      // or :after content.  Neither can non-elements or leaf frames.
      if (!pseudoTag && localContent &&
          localContent->IsNodeOfType(nsINode::eELEMENT) &&
          !aFrame->IsLeaf()) {
        // Check for a new :before pseudo and an existing :before
        // frame, but only if the frame is the first continuation.
        nsIFrame* prevContinuation = aFrame->GetPrevContinuation();
        if (!prevContinuation) {
          // Checking for a :before frame is cheaper than getting the
          // :before style context.
          if (!nsLayoutUtils::GetBeforeFrame(aFrame) &&
              nsLayoutUtils::HasPseudoStyle(localContent, newContext,
                                            nsCSSPseudoElements::before,
                                            aPresContext)) {
            // Have to create the new :before frame
            NS_UpdateHint(aMinChange, nsChangeHint_ReconstructFrame);
            aChangeList->AppendChange(aFrame, content,
                                      nsChangeHint_ReconstructFrame);
          }
        }
      }
    }

    
    if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
      // Make sure not to do this for pseudo-frames -- those can't have :before
      // or :after content.  Neither can non-elements or leaf frames.
      if (!pseudoTag && localContent &&
          localContent->IsNodeOfType(nsINode::eELEMENT) &&
          !aFrame->IsLeaf()) {
        // Check for new :after content, but only if the frame is the
        // last continuation.
        nsIFrame* nextContinuation = aFrame->GetNextContinuation();

        if (!nextContinuation) {
          // Getting the :after frame is more expensive than getting the pseudo
          // context, so get the pseudo context first.
          if (nsLayoutUtils::HasPseudoStyle(localContent, newContext,
                                            nsCSSPseudoElements::after,
                                            aPresContext) &&
              !nsLayoutUtils::GetAfterFrame(aFrame)) {
            // have to create the new :after frame
            NS_UpdateHint(aMinChange, nsChangeHint_ReconstructFrame);
            aChangeList->AppendChange(aFrame, content,
                                      nsChangeHint_ReconstructFrame);
          }
        }      
      }
    }
    
    if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
      
      // There is no need to waste time crawling into a frame's children on a frame change.
      // The act of reconstructing frames will force new style contexts to be resolved on all
      // of this frame's descendants anyway, so we want to avoid wasting time processing
      // style contexts that we're just going to throw away anyway. - dwh

      // now do children
      PRInt32 listIndex = 0;
      nsIAtom* childList = nsnull;

      do {
        nsIFrame* child = aFrame->GetFirstChild(childList);
        while (child) {
          if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
            // only do frames that are in flow
            if (nsGkAtoms::placeholderFrame == child->GetType()) { // placeholder
              // get out of flow frame and recur there
              nsIFrame* outOfFlowFrame =
                nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
              NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
              NS_ASSERTION(outOfFlowFrame != resolvedChild,
                           "out-of-flow frame not a true descendant");

              // Note that the out-of-flow may not be a geometric descendant of
              // the frame where we started the reresolve.  Therefore, even if
              // aMinChange already includes nsChangeHint_ReflowFrame we don't
              // want to pass that on to the out-of-flow reresolve, since that
              // can lead to the out-of-flow not getting reflown when it should
              // be (eg a reresolve starting at <body> that involves reflowing
              // the <body> would miss reflowing fixed-pos nodes that also need
              // reflow).  In the cases when the out-of-flow _is_ a geometric
              // descendant of a frame we already have a reflow hint for,
              // reflow coalescing should keep us from doing the work twice.

              // |nsFrame::GetParentStyleContextFrame| checks being out
              // of flow so that this works correctly.
              ReResolveStyleContext(aPresContext, outOfFlowFrame,
                                    content, aChangeList,
                                    NS_SubtractHint(aMinChange,
                                                    nsChangeHint_ReflowFrame));

              // reresolve placeholder's context under the same parent
              // as the out-of-flow frame
              ReResolveStyleContext(aPresContext, child, content,
                                    aChangeList, aMinChange);
            }
            else {  // regular child frame
              if (child != resolvedChild) {
                ReResolveStyleContext(aPresContext, child, content,
                                      aChangeList, aMinChange);
              } else {
                NOISY_TRACE_FRAME("child frame already resolved as descendant, skipping",aFrame);
              }
            }
          }
          child = child->GetNextSibling();
        }

        childList = aFrame->GetAdditionalChildListName(listIndex++);
      } while (childList);
      // XXX need to do overflow frames???
    }

    newContext->Release();
  }

#ifdef ACCESSIBILITY
  if (isAccessibilityActive &&
      aFrame->GetStyleVisibility()->IsVisible() != isVisible) {
    // XXX Visibility does not affect descendents with visibility set
    // Work on a separate, accurate mechanism for dealing with visibility changes.
    // A significant enough change occured that this part
    // of the accessible tree is no longer valid.
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->InvalidateSubtreeFor(mPresShell, aFrame->GetContent(),
                                       isVisible ? nsIAccessibleEvent::EVENT_HIDE :
                                                   nsIAccessibleEvent::EVENT_SHOW);
    }
  }
#endif

  return aMinChange;
}

nsChangeHint
nsFrameManager::ComputeStyleChangeFor(nsIFrame          *aFrame, 
                                      nsStyleChangeList *aChangeList,
                                      nsChangeHint       aMinChange)
{
  nsChangeHint topLevelChange = aMinChange;

  nsIFrame* frame = aFrame;
  nsIFrame* frame2 = aFrame;

  NS_ASSERTION(!frame->GetPrevContinuation(), "must start with the first in flow");

  // We want to start with this frame and walk all its next-in-flows,
  // as well as all its special siblings and their next-in-flows,
  // reresolving style on all the frames we encounter in this walk.

  nsPropertyTable *propTable = GetPresContext()->PropertyTable();

  do {
    // Outer loop over special siblings
    do {
      // Inner loop over next-in-flows of the current frame
      nsChangeHint frameChange =
        ReResolveStyleContext(GetPresContext(), frame, nsnull,
                              aChangeList, topLevelChange);
      NS_UpdateHint(topLevelChange, frameChange);

      if (topLevelChange & nsChangeHint_ReconstructFrame) {
        // If it's going to cause a framechange, then don't bother
        // with the continuations or special siblings since they'll be
        // clobbered by the frame reconstruct anyway.
        NS_ASSERTION(!frame->GetPrevContinuation(),
                     "continuing frame had more severe impact than first-in-flow");
        return topLevelChange;
      }

      frame = frame->GetNextContinuation();
    } while (frame);

    // Might we have special siblings?
    if (!(frame2->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
      // nothing more to do here
      return topLevelChange;
    }
    
    frame2 = NS_STATIC_CAST(nsIFrame*,
         propTable->GetProperty(frame2, nsGkAtoms::IBSplitSpecialSibling));
    frame = frame2;
  } while (frame2);
  return topLevelChange;
}


nsReStyleHint
nsFrameManager::HasAttributeDependentStyle(nsIContent *aContent,
                                           nsIAtom *aAttribute,
                                           PRInt32 aModType)
{
  nsReStyleHint hint = mStyleSet->HasAttributeDependentStyle(GetPresContext(),
                                                             aContent,
                                                             aAttribute,
                                                             aModType);

  if (aAttribute == nsGkAtoms::style) {
    // Perhaps should check that it's XUL, SVG, (or HTML) namespace, but
    // it doesn't really matter.  Or we could even let
    // HTMLCSSStyleSheetImpl::HasAttributeDependentStyle handle it.
    hint = nsReStyleHint(hint | eReStyle_Self);
  }

  return hint;
}

// Capture state for a given frame.
// Accept a content id here, in some cases we may not have content (scroll position)
void
nsFrameManager::CaptureFrameStateFor(nsIFrame* aFrame,
                                     nsILayoutHistoryState* aState,
                                     nsIStatefulFrame::SpecialStateID aID)
{
  if (!aFrame || !aState) {
    NS_WARNING("null frame, or state");
    return;
  }

  // Only capture state for stateful frames
  nsIStatefulFrame* statefulFrame;
  CallQueryInterface(aFrame, &statefulFrame);

  if (!statefulFrame) {
    return;
  }

  // Capture the state, exit early if we get null (nothing to save)
  nsAutoPtr<nsPresState> frameState;
  nsresult rv = statefulFrame->SaveState(aID, getter_Transfers(frameState));
  if (!frameState) {
    return;
  }

  // Generate the hash key to store the state under
  // Exit early if we get empty key
  nsCAutoString stateKey;
  nsIContent* content = aFrame->GetContent();
  nsIDocument* doc = content ? content->GetCurrentDoc() : nsnull;
  rv = nsContentUtils::GenerateStateKey(content, doc, aID, stateKey);
  if(NS_FAILED(rv) || stateKey.IsEmpty()) {
    return;
  }

  // Store the state
  rv = aState->AddState(stateKey, frameState);
  if (NS_SUCCEEDED(rv)) {
    // aState owns frameState now.
    frameState.forget();
  }
}

void
nsFrameManager::CaptureFrameState(nsIFrame* aFrame,
                                  nsILayoutHistoryState* aState)
{
  NS_PRECONDITION(nsnull != aFrame && nsnull != aState, "null parameters passed in");

  CaptureFrameStateFor(aFrame, aState);

  // Now capture state recursively for the frame hierarchy rooted at aFrame
  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;
  do {    
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {             
      CaptureFrameState(childFrame, aState);
      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }
    childListName = aFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
}

// Restore state for a given frame.
// Accept a content id here, in some cases we may not have content (scroll position)
void
nsFrameManager::RestoreFrameStateFor(nsIFrame* aFrame,
                                     nsILayoutHistoryState* aState,
                                     nsIStatefulFrame::SpecialStateID aID)
{
  if (!aFrame || !aState) {
    NS_WARNING("null frame or state");
    return;
  }

  // Only capture state for stateful frames
  nsIStatefulFrame* statefulFrame;
  CallQueryInterface(aFrame, &statefulFrame);
  if (!statefulFrame) {
    return;
  }

  // Generate the hash key the state was stored under
  // Exit early if we get empty key
  nsIContent* content = aFrame->GetContent();
  // If we don't have content, we can't generate a hash
  // key and there's probably no state information for us.
  if (!content) {
    return;
  }

  nsCAutoString stateKey;
  nsIDocument* doc = content->GetCurrentDoc();
  nsresult rv = nsContentUtils::GenerateStateKey(content, doc, aID, stateKey);
  if (NS_FAILED(rv) || stateKey.IsEmpty()) {
    return;
  }

  // Get the state from the hash
  nsPresState *frameState;
  rv = aState->GetState(stateKey, &frameState);
  if (!frameState) {
    return;
  }

  // Restore it
  rv = statefulFrame->RestoreState(frameState);
  if (NS_FAILED(rv)) {
    return;
  }

  // If we restore ok, remove the state from the state table
  aState->RemoveState(stateKey);
}

void
nsFrameManager::RestoreFrameState(nsIFrame* aFrame,
                                  nsILayoutHistoryState* aState)
{
  NS_PRECONDITION(nsnull != aFrame && nsnull != aState, "null parameters passed in");
  
  RestoreFrameStateFor(aFrame, aState);

  // Now restore state recursively for the frame hierarchy rooted at aFrame
  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;
  do {    
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {
      RestoreFrameState(childFrame, aState);
      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }
    childListName = aFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
}

//----------------------------------------------------------------------

static PLHashNumber
HashKey(void* key)
{
  return NS_PTR_TO_INT32(key);
}

static PRIntn
CompareKeys(void* key1, void* key2)
{
  return key1 == key2;
}

//----------------------------------------------------------------------

nsFrameManagerBase::UndisplayedMap::UndisplayedMap(PRUint32 aNumBuckets)
{
  MOZ_COUNT_CTOR(nsFrameManagerBase::UndisplayedMap);
  mTable = PL_NewHashTable(aNumBuckets, (PLHashFunction)HashKey,
                           (PLHashComparator)CompareKeys,
                           (PLHashComparator)nsnull,
                           nsnull, nsnull);
  mLastLookup = nsnull;
}

nsFrameManagerBase::UndisplayedMap::~UndisplayedMap(void)
{
  MOZ_COUNT_DTOR(nsFrameManagerBase::UndisplayedMap);
  Clear();
  PL_HashTableDestroy(mTable);
}

PLHashEntry**  
nsFrameManagerBase::UndisplayedMap::GetEntryFor(nsIContent* aParentContent)
{
  if (mLastLookup && (aParentContent == (*mLastLookup)->key)) {
    return mLastLookup;
  }
  PLHashNumber hashCode = NS_PTR_TO_INT32(aParentContent);
  PLHashEntry** entry = PL_HashTableRawLookup(mTable, hashCode, aParentContent);
  if (*entry) {
    mLastLookup = entry;
  }
  return entry;
}

UndisplayedNode* 
nsFrameManagerBase::UndisplayedMap::GetFirstNode(nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  if (*entry) {
    return (UndisplayedNode*)((*entry)->value);
  }
  return nsnull;
}

void
nsFrameManagerBase::UndisplayedMap::AppendNodeFor(UndisplayedNode* aNode,
                                                  nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  if (*entry) {
    UndisplayedNode*  node = (UndisplayedNode*)((*entry)->value);
    while (node->mNext) {
      if (node->mContent == aNode->mContent) {
        // We actually need to check this in optimized builds because
        // there are some callers that do this.  See bug 118014, bug
        // 136704, etc.
        NS_NOTREACHED("node in map twice");
        delete aNode;
        return;
      }
      node = node->mNext;
    }
    node->mNext = aNode;
  }
  else {
    PLHashNumber hashCode = NS_PTR_TO_INT32(aParentContent);
    PL_HashTableRawAdd(mTable, entry, hashCode, aParentContent, aNode);
    mLastLookup = nsnull; // hashtable may have shifted bucket out from under us
  }
}

nsresult 
nsFrameManagerBase::UndisplayedMap::AddNodeFor(nsIContent* aParentContent,
                                               nsIContent* aChild, 
                                               nsStyleContext* aStyle)
{
  UndisplayedNode*  node = new UndisplayedNode(aChild, aStyle);
  if (! node) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AppendNodeFor(node, aParentContent);
  return NS_OK;
}

void
nsFrameManagerBase::UndisplayedMap::RemoveNodeFor(nsIContent* aParentContent,
                                                  UndisplayedNode* aNode)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  NS_ASSERTION(*entry, "content not in map");
  if (*entry) {
    if ((UndisplayedNode*)((*entry)->value) == aNode) {  // first node
      if (aNode->mNext) {
        (*entry)->value = aNode->mNext;
        aNode->mNext = nsnull;
      }
      else {
        PL_HashTableRawRemove(mTable, entry, *entry);
        mLastLookup = nsnull; // hashtable may have shifted bucket out from under us
      }
    }
    else {
      UndisplayedNode*  node = (UndisplayedNode*)((*entry)->value);
      while (node->mNext) {
        if (node->mNext == aNode) {
          node->mNext = aNode->mNext;
          aNode->mNext = nsnull;
          break;
        }
        node = node->mNext;
      }
    }
  }
  delete aNode;
}

void
nsFrameManagerBase::UndisplayedMap::RemoveNodesFor(nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  NS_ASSERTION(entry, "content not in map");
  if (*entry) {
    UndisplayedNode*  node = (UndisplayedNode*)((*entry)->value);
    NS_ASSERTION(node, "null node for non-null entry in UndisplayedMap");
    delete node;
    PL_HashTableRawRemove(mTable, entry, *entry);
    mLastLookup = nsnull; // hashtable may have shifted bucket out from under us
  }
}

PR_STATIC_CALLBACK(PRIntn)
RemoveUndisplayedEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  UndisplayedNode*  node = (UndisplayedNode*)(he->value);
  delete node;
  // Remove and free this entry and continue enumerating
  return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
}

void
nsFrameManagerBase::UndisplayedMap::Clear(void)
{
  mLastLookup = nsnull;
  PL_HashTableEnumerateEntries(mTable, RemoveUndisplayedEntry, 0);
}
