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
#include "nscore.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsCSSFrameConstructor.h"
#include "nsStyleContext.h"
#include "nsStyleChangeList.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "plhash.h"
#include "nsPlaceholderFrame.h"
#include "nsLayoutAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsHTMLAtoms.h"
#ifdef NS_DEBUG
#include "nsISupportsArray.h"
#include "nsIStyleRule.h"
#endif
#include "nsILayoutHistoryState.h"
#include "nsIPresState.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLBinding.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
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
#include "nsDummyLayoutRequest.h"
#include "nsLayoutErrors.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"
#include "imgIRequest.h"

#include "nsFrameManager.h"

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

// Class IID's
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// IID's

//----------------------------------------------------------------------

struct PlaceholderMapEntry : public PLDHashEntryHdr {
  // key (the out of flow frame) can be obtained through placeholder frame
  nsPlaceholderFrame *placeholderFrame;
};

PR_STATIC_CALLBACK(const void *)
PlaceholderMapGetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  PlaceholderMapEntry *entry = NS_STATIC_CAST(PlaceholderMapEntry*, hdr);
  return entry->placeholderFrame->GetOutOfFlowFrame();
}

PR_STATIC_CALLBACK(PRBool)
PlaceholderMapMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const PlaceholderMapEntry *entry =
    NS_STATIC_CAST(const PlaceholderMapEntry*, hdr);
  return entry->placeholderFrame->GetOutOfFlowFrame() == key;
}

static PLDHashTableOps PlaceholderMapOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PlaceholderMapGetKey,
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
PR_STATIC_CALLBACK(const void *)
PrimaryFrameMapGetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*, hdr);
  return entry->frame->GetContent();
}

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
  PrimaryFrameMapGetKey,
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
MOZ_DECL_CTOR_COUNTER(UndisplayedNode)

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

  // A CantRenderReplacedElementEvent has a weak pointer to the frame
  // manager, and the frame manager has a weak pointer to the event.
  // The event queue owns the event and the FrameManager will delete
  // the event if it's going to go away.
struct CantRenderReplacedElementEvent : public PLEvent {
  CantRenderReplacedElementEvent(nsFrameManager* aFrameManager, nsIFrame* aFrame, nsIPresShell* aPresShell) NS_HIDDEN;
  ~CantRenderReplacedElementEvent() NS_HIDDEN;
  // XXXldb Should the pres shell maintain a reference count on a single
  // dummy layout request instead of doing creation of a separate one
  // here (and per-event!)?
  NS_HIDDEN_(nsresult) AddLoadGroupRequest(nsIPresShell* aPresShell);
  NS_HIDDEN_(nsresult) RemoveLoadGroupRequest();

  nsIFrame*  mFrame;                     // the frame that can't be rendered
  CantRenderReplacedElementEvent* mNext; // next event in the list
  nsCOMPtr<nsIRequest> mDummyLayoutRequest; // load group request
  nsWeakPtr mPresShell;                     // for removing load group request later
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

  nsPresContext *presContext = mPresShell->GetPresContext();
  
  // Destroy the frame hierarchy.
  mPresShell->SetIgnoreFrameDestruction(PR_TRUE);

  mIsDestroyingFrames = PR_TRUE;  // This flag prevents GetPrimaryFrameFor from returning pointers to destroyed frames

  if (mRootFrame) {
    mRootFrame->Destroy(presContext);
    mRootFrame = nsnull;
  }
  
  if (mPrimaryFrameMap.ops) {
    PL_DHashTableFinish(&mPrimaryFrameMap);
    mPrimaryFrameMap.ops = nsnull;
  }
  if (mPlaceholderMap.ops) {
    PL_DHashTableFinish(&mPlaceholderMap);
    mPlaceholderMap.ops = nsnull;
  }
  delete mUndisplayedMap;

  // If we're not going to be used anymore, we should revoke the
  // pending |CantRenderReplacedElementEvent|s being sent to us.
#ifdef DEBUG
  nsresult rv =
#endif
    RevokePostedEvents();
  NS_ASSERTION(NS_SUCCEEDED(rv), "RevokePostedEvents failed:  might crash");

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
        if (siblingFrame->GetType() == nsLayoutAtoms::canvasFrame) {
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
nsFrameManager::GetPrimaryFrameFor(nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent, nsnull);

  if (mIsDestroyingFrames) {
#ifdef DEBUG
    printf("GetPrimaryFrameFor() called while nsFrameManager is being destroyed!\n");
#endif
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

    // Give the frame construction code the opportunity to return the
    // frame that maps the content object
    nsPresContext *presContext = mPresShell->GetPresContext();
    if (!presContext) {
      return nsnull;
    }

    // if the prev sibling of aContent has a cached primary frame,
    // pass that data in to the style set to speed things up
    // if any methods in here fail, don't report that failure
    // we're just trying to enhance performance here, not test for correctness
    nsFindFrameHint hint;
    nsIContent* parent = aContent->GetParent();
    if (parent)
    {
      PRInt32 index = parent->IndexOf(aContent);
      if (index > 0)  // no use looking if it's the first child
      {
        nsIContent *prevSibling;
        nsIAtom *tag;
        do {
          prevSibling = parent->GetChildAt(--index);
          tag = prevSibling->Tag();
        } while (index &&
                 (tag == nsLayoutAtoms::textTagName ||
                  tag == nsLayoutAtoms::commentTagName ||
                  tag == nsLayoutAtoms::processingInstructionTagName));
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
      FindPrimaryFrameFor(presContext, this, aContent, &result, 
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
  // it's ok if aPrimaryFrame is null

  // If aPrimaryFrame is NULL, then remove the mapping
  if (!aPrimaryFrame) {
    if (mPrimaryFrameMap.ops) {
      PL_DHashTableOperate(&mPrimaryFrameMap, aContent, PL_DHASH_REMOVE);
    }
  } else {
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
  }
    
  return NS_OK;
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
  NS_PRECONDITION(nsLayoutAtoms::placeholderFrame == aPlaceholderFrame->GetType(),
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
  NS_PRECONDITION(nsLayoutAtoms::placeholderFrame == aPlaceholderFrame->GetType(),
                  "unexpected frame type");

  /*
   * nsCSSFrameConstructor::ReconstructDocElementHierarchy calls
   * ClearPlaceholderFrameMap and _then_ removes the fixed-positioned
   * frames one by one.  As these are removed they call
   * UnregisterPlaceholderFrame on their placeholders, but this is all
   * happening when mPlaceholderMap is already finished, so there is
   * nothing to do here.  See bug 144479.
   */
  if (mPlaceholderMap.ops) {
    PL_DHashTableOperate(&mPlaceholderMap,
                         aPlaceholderFrame->GetOutOfFlowFrame(),
                         PL_DHASH_REMOVE);
  }
}

void
nsFrameManager::ClearPlaceholderFrameMap()
{
  if (mPlaceholderMap.ops) {
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
  nsIPresShell *presShell = GetPresShell();
  nsPresContext *presContext = presShell->GetPresContext();
#ifdef IBMBIDI
  if (aPrevFrame) {
    // Insert aFrameList after the last bidi continuation of aPrevFrame.
    nsPropertyTable *propTable = presContext->PropertyTable();
    nsIFrame* nextBidi;
    for (; ;) {
      nextBidi = NS_STATIC_CAST(nsIFrame*,
                  propTable->GetProperty(aPrevFrame, nsLayoutAtoms::nextBidi));
      if (!nextBidi) {
        break;
      }
      aPrevFrame = nextBidi;
    }
  }
#endif // IBMBIDI

  return aParentFrame->InsertFrames(GetPresContext(), *presShell,
                                    aListName, aPrevFrame, aFrameList);
}

nsresult
nsFrameManager::RemoveFrame(nsIFrame*       aParentFrame,
                            nsIAtom*        aListName,
                            nsIFrame*       aOldFrame)
{
#ifdef IBMBIDI
  // Don't let the parent remove next bidi. In the other cases the it should NOT be removed.
  nsIFrame* nextBidi =
    NS_STATIC_CAST(nsIFrame*, aOldFrame->GetProperty(nsLayoutAtoms::nextBidi));
  if (nextBidi) {
    RemoveFrame(aParentFrame, aListName, nextBidi);
  }
#endif // IBMBIDI

  return aParentFrame->RemoveFrame(GetPresContext(), *GetPresShell(),
                                   aListName, aOldFrame);
}

//----------------------------------------------------------------------

void
nsFrameManager::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // Dequeue and destroy and posted events for this frame
  DequeuePostedEventFor(aFrame);

#ifdef DEBUG
  if (mPrimaryFrameMap.ops) {
    PrimaryFrameMapEntry *entry = NS_STATIC_CAST(PrimaryFrameMapEntry*,
        PL_DHashTableOperate(&mPrimaryFrameMap, aFrame->GetContent(), PL_DHASH_LOOKUP));
    NS_ASSERTION(!PL_DHASH_ENTRY_IS_BUSY(entry) || entry->frame != aFrame,
                 "frame was not removed from primary frame map before "
                 "destruction or was readded to map after being removed");
  }
#endif
}

nsresult
nsFrameManager::RevokePostedEvents()
{
  nsresult rv = NS_OK;
#ifdef NOISY_EVENTS
  printf("%p ~RevokePostedEvents() start\n", this);
#endif
  if (mPostedEvents) {
    mPostedEvents = nsnull;

    // Revoke any events in the event queue that are owned by us
    nsCOMPtr<nsIEventQueueService> eventService =
      do_GetService(kEventQueueServiceCID, &rv);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                             getter_AddRefs(eventQueue));

      if (NS_SUCCEEDED(rv) && eventQueue) {
        rv = eventQueue->RevokeEvents(this);
      }
    }
  }
#ifdef NOISY_EVENTS
  printf("%p ~RevokePostedEvents() end\n", this);
#endif
  return rv;
}

CantRenderReplacedElementEvent**
nsFrameManager::FindPostedEventFor(nsIFrame* aFrame)
{
  CantRenderReplacedElementEvent** event = &mPostedEvents;

  while (*event) {
    if ((*event)->mFrame == aFrame) {
      return event;
    }
    event = &(*event)->mNext;
  }

  return event;
}

void
nsFrameManager::DequeuePostedEventFor(nsIFrame* aFrame)
{
  // If there's a posted event for this frame, then remove it
  CantRenderReplacedElementEvent** event = FindPostedEventFor(aFrame);
  if (*event) {
    CantRenderReplacedElementEvent* tmp = *event;

    // Remove it from our linked list of posted events
    *event = (*event)->mNext;
    
    // Dequeue it from the event queue
    nsresult              rv;

    nsCOMPtr<nsIEventQueueService> eventService =
      do_GetService(kEventQueueServiceCID, &rv);

    NS_ASSERTION(NS_SUCCEEDED(rv),
            "will crash soon due to event holding dangling pointer to frame");
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                             getter_AddRefs(eventQueue));

      NS_ASSERTION(NS_SUCCEEDED(rv) && eventQueue,
            "will crash soon due to event holding dangling pointer to frame");
      if (NS_SUCCEEDED(rv) && eventQueue) {
        PLEventQueue* plqueue;

        eventQueue->GetPLEventQueue(&plqueue);
        NS_ASSERTION(plqueue,
            "will crash soon due to event holding dangling pointer to frame");
        if (plqueue) {
          // Remove the event and then destroy it
          PL_DequeueEvent(tmp, plqueue);
          PL_DestroyEvent(tmp);
        }
      }
    }
  }
}

void
nsFrameManager::HandlePLEvent(CantRenderReplacedElementEvent* aEvent)
{
#ifdef NOISY_EVENTS
  printf("nsFrameManager::HandlePLEvent() start for FM %p\n", aEvent->owner);
#endif
  nsFrameManager* frameManager = (nsFrameManager*)aEvent->owner;
  NS_ASSERTION(frameManager, "null frame manager");

  if (!frameManager->mPresShell) {
    NS_ASSERTION(frameManager->mPresShell,
                 "event not removed from queue on shutdown");
    return;
  }

  // Remove the posted event from the linked list
  CantRenderReplacedElementEvent** events = &frameManager->mPostedEvents;
  while (*events) {
    if (*events == aEvent) {
      *events = (*events)->mNext;
      break;
    }
    events = &(*events)->mNext;
    NS_ASSERTION(*events, "event not in queue");
  }

  // Notify the style system and then process any reflow commands that
  // are generated
  nsIPresShell *shell = frameManager->mPresShell;
  shell->FrameConstructor()->
    CantRenderReplacedElement(shell, shell->GetPresContext(), aEvent->mFrame);

#ifdef NOISY_EVENTS
  printf("nsFrameManager::HandlePLEvent() end for FM %p\n", aEvent->owner);
#endif
}

void
nsFrameManager::DestroyPLEvent(CantRenderReplacedElementEvent* aEvent)
{
  delete aEvent;
}

CantRenderReplacedElementEvent::CantRenderReplacedElementEvent(nsFrameManager* aFrameManager,
                                                               nsIFrame*     aFrame,
                                                               nsIPresShell* aPresShell)
{
  PL_InitEvent(this, aFrameManager,
               (PLHandleEventProc)&nsFrameManager::HandlePLEvent,
               (PLDestroyEventProc)&nsFrameManager::DestroyPLEvent);
  mFrame = aFrame;
  
  if (nsLayoutAtoms::objectFrame == aFrame->GetType()) {
    AddLoadGroupRequest(aPresShell);
  }
}

CantRenderReplacedElementEvent::~CantRenderReplacedElementEvent()
{
  RemoveLoadGroupRequest();
}

// Add a load group request in order to delay the onLoad handler when we have
// pending replacements
nsresult
CantRenderReplacedElementEvent::AddLoadGroupRequest(nsIPresShell* aPresShell)
{
  nsIDocument *doc = aPresShell->GetDocument();
  if (!doc) return NS_ERROR_FAILURE;

  nsresult rv = nsDummyLayoutRequest::Create(getter_AddRefs(mDummyLayoutRequest), aPresShell);
  if (NS_FAILED(rv)) return rv;
  if (!mDummyLayoutRequest) return NS_ERROR_FAILURE;

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
  if (!loadGroup) return NS_ERROR_FAILURE;
  
  rv = mDummyLayoutRequest->SetLoadGroup(loadGroup);
  if (NS_FAILED(rv)) return rv;
  
  mPresShell = do_GetWeakReference(aPresShell);

  return loadGroup->AddRequest(mDummyLayoutRequest, nsnull);
}

// Remove the load group request added above
nsresult
CantRenderReplacedElementEvent::RemoveLoadGroupRequest()
{
  nsresult rv = NS_OK;

  if (mDummyLayoutRequest) {
    nsCOMPtr<nsIRequest> request = mDummyLayoutRequest;
    mDummyLayoutRequest = nsnull;

    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
    if (!presShell) return NS_ERROR_FAILURE;
    
    nsIDocument *doc = presShell->GetDocument();
    if (!doc) return NS_ERROR_FAILURE;;

    nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
    if (!loadGroup) return NS_ERROR_FAILURE;

    rv = loadGroup->RemoveRequest(request, nsnull, NS_OK);
  }
  return rv;
}

nsresult
nsFrameManager::CantRenderReplacedElement(nsIFrame* aFrame)
{
#ifdef NOISY_EVENTS
  printf("%p nsFrameManager::CantRenderReplacedElement called\n", this);
#endif

  // We need to notify the style stystem, but post the notification so it
  // doesn't happen now
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> eventService =
    do_GetService(kEventQueueServiceCID, &rv);

  if (eventService) {
    nsCOMPtr<nsIEventQueue> eventQueue;
    rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                           getter_AddRefs(eventQueue));

    if (NS_SUCCEEDED(rv) && eventQueue) {
      // Verify that there isn't already a posted event associated with
      // this frame.
      if (*FindPostedEventFor(aFrame))
        return NS_OK;

      CantRenderReplacedElementEvent* ev;

      // Create a new event
      ev = new CantRenderReplacedElementEvent(this, aFrame, mPresShell);

      // Post the event
      rv = eventQueue->PostEvent(ev);
      if (NS_FAILED(rv)) {
        NS_ERROR("failed to post event");
        PL_DestroyEvent(ev);
      }
      else {
        // Add the event to our linked list of posted events
        ev->mNext = mPostedEvents;
        mPostedEvents = ev;
      }
    }
  }

  return rv;
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
      fputs(NS_LossyConvertUCS2toASCII(name).get(), stdout);
    }
    fprintf(stdout, " (%p)", NS_STATIC_CAST(void*, aFrame));
  }
  if (aContext) {
    fprintf(stdout, " style: %p ", NS_STATIC_CAST(void*, aContext));

    nsIAtom* pseudoTag = aContext->GetPseudoType();
    if (pseudoTag) {
      nsAutoString  buffer;
      pseudoTag->ToString(buffer);
      fputs(NS_LossyConvertUCS2toASCII(buffer).get(), stdout);
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
        if (nsLayoutAtoms::placeholderFrame == child->GetType()) { 
          // placeholder: first recirse and verify the out of flow frame,
          // then verify the placeholder's context
          nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

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
nsFrameManager::ReParentStyleContext(nsIFrame* aFrame, 
                                     nsStyleContext* aNewParentContext)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aFrame) {
    // DO NOT verify the style tree before reparenting.  The frame
    // tree has already been changed, so this check would just fail.
    nsStyleContext* oldContext = aFrame->GetStyleContext();
    if (oldContext) {
      nsPresContext *presContext = GetPresContext();
      nsRefPtr<nsStyleContext> newContext;
      result = NS_OK;
      newContext = mStyleSet->ReParentStyleContext(presContext, oldContext,
                                                   aNewParentContext);
      if (newContext) {
        if (newContext != oldContext) {
          PRInt32 listIndex = 0;
          nsIAtom* childList = nsnull;
          nsIFrame* child;
          
          aFrame->SetStyleContext(presContext, newContext);

          do {
            child = aFrame->GetFirstChild(childList);
            while (child) {
              if (NS_FRAME_OUT_OF_FLOW != (child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
                // only do frames that are in flow
                if (nsLayoutAtoms::placeholderFrame == child->GetType()) {
                  // get out of flow frame and recurse there
                  nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
                  NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

                  result = ReParentStyleContext(outOfFlowFrame, newContext);

                  // reparent placeholder's context under out of flow frame
                  nsStyleContext* outOfFlowContext = outOfFlowFrame->GetStyleContext();
                  ReParentStyleContext(child, outOfFlowContext);
                }
                else { // regular frame
                  result = ReParentStyleContext(child, newContext);
                }
              }

              child = child->GetNextSibling();
            }

            childList = aFrame->GetAdditionalChildListName(listIndex++);
          } while (childList);

          // do additional contexts 
          PRInt32 contextIndex = -1;
          while (1) {
            nsStyleContext* oldExtraContext = aFrame->GetAdditionalStyleContext(++contextIndex);
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
              result = NS_OK; // ok not to have extras (or run out)
              break;
            }
          }
#ifdef DEBUG
          VerifyStyleTree(GetPresContext(), aFrame, aNewParentContext);
#endif
        }
      }
    }
  }
  return result;
}

static nsChangeHint
CaptureChange(nsStyleContext* aOldContext, nsStyleContext* aNewContext,
              nsIFrame* aFrame, nsIContent* aContent,
              nsStyleChangeList* aChangeList, nsChangeHint aMinChange)
{
  nsChangeHint ourChange = NS_STYLE_HINT_NONE;
  ourChange = aOldContext->CalcStyleDifference(aNewContext);
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

  nsChangeHint resultChange = NS_STYLE_HINT_NONE;
  nsStyleContext* oldContext = aFrame->GetStyleContext();
  nsStyleSet* styleSet = aPresContext->StyleSet();

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
      // resolve the provider here (before aFrame below)
      resultChange = ReResolveStyleContext(aPresContext, providerFrame,
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
                                   content, aChangeList, aMinChange);
        if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
          // if frame gets regenerated, let it keep old context
          aFrame->SetStyleContext(aPresContext, newContext);
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
                                       aMinChange);
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

    resultChange = aMinChange;

    if (!(aMinChange & nsChangeHint_ReconstructFrame)) {
      // Make sure not to do this for pseudo-frames -- those can't have :before
      // or :after content.
      if (!pseudoTag && localContent &&
          localContent->IsContentOfType(nsIContent::eELEMENT)) {
        // Check for a new :before pseudo and an existing :before
        // frame, but only if the frame is the first-in-flow.
        nsIFrame* prevInFlow = aFrame->GetPrevInFlow();
        if (!prevInFlow) {
          // Checking for a :before frame is cheaper than getting the
          // :before style context.
          if (!nsLayoutUtils::GetBeforeFrame(aFrame, aPresContext) &&
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
      // or :after content.
      if (!pseudoTag && localContent &&
          localContent->IsContentOfType(nsIContent::eELEMENT)) {
        // Check for new :after content, but only if the frame is the
        // first-in-flow.
        nsIFrame* nextInFlow = aFrame->GetNextInFlow();

        if (!nextInFlow) {
          // Getting the :after frame is more expensive than getting the pseudo
          // context, so get the pseudo context first.
          if (nsLayoutUtils::HasPseudoStyle(localContent, newContext,
                                            nsCSSPseudoElements::after,
                                            aPresContext) &&
              !nsLayoutUtils::GetAfterFrame(aFrame, aPresContext)) {
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
            if (nsLayoutAtoms::placeholderFrame == child->GetType()) { // placeholder
              // get out of flow frame and recur there
              nsIFrame* outOfFlowFrame =
                NS_STATIC_CAST(nsPlaceholderFrame*,child)->GetOutOfFlowFrame();
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
                NOISY_TRACE_FRAME("child frame already resolved as descendent, skipping",aFrame);
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

  return resultChange;
}

nsChangeHint
nsFrameManager::ComputeStyleChangeFor(nsIFrame          *aFrame, 
                                      nsStyleChangeList *aChangeList,
                                      nsChangeHint       aMinChange)
{
  nsChangeHint topLevelChange = aMinChange;

  nsIFrame* frame = aFrame;
  nsIFrame* frame2 = aFrame;

  NS_ASSERTION(!frame->GetPrevInFlow(), "must start with the first in flow");

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
        NS_ASSERTION(!frame->GetPrevInFlow(),
                     "continuing frame had more severe impact than first-in-flow");
        return topLevelChange;
      }

      frame = frame->GetNextInFlow();
    } while (frame);

    // Might we have special siblings?
    if (!(frame2->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
      // nothing more to do here
      return topLevelChange;
    }
    
    frame2 = NS_STATIC_CAST(nsIFrame*,
         propTable->GetProperty(frame2, nsLayoutAtoms::IBSplitSpecialSibling));
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

  if (aAttribute == nsHTMLAtoms::style) {
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
  nsCOMPtr<nsIPresState> frameState;
  nsresult rv = NS_OK;
  rv = statefulFrame->SaveState(GetPresContext(), getter_AddRefs(frameState));
  if (!frameState) {
    return;
  }

  // Generate the hash key to store the state under
  // Exit early if we get empty key
  nsCAutoString stateKey;
  rv = nsContentUtils::GenerateStateKey(aFrame->GetContent(), aID, stateKey);
  if(NS_FAILED(rv) || stateKey.IsEmpty()) {
    return;
  }

  // Store the state
  aState->AddState(stateKey, frameState);
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
  nsresult rv = nsContentUtils::GenerateStateKey(content, aID, stateKey);
  if (NS_FAILED(rv) || stateKey.IsEmpty()) {
    return;
  }

  // Get the state from the hash
  nsCOMPtr<nsIPresState> frameState;
  rv = aState->GetState(stateKey, getter_AddRefs(frameState));
  if (!frameState) {
    return;
  }

  // Restore it
  rv = statefulFrame->RestoreState(GetPresContext(), frameState);
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

MOZ_DECL_CTOR_COUNTER(UndisplayedMap)

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
