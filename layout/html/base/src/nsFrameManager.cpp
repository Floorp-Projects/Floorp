/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nscore.h"
#include "nsIFrameManager.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleContext.h"
#include "nsStyleChangeList.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "plhash.h"
#include "nsDST.h"
#include "nsPlaceholderFrame.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#ifdef NS_DEBUG
#include "nsISupportsArray.h"
#include "nsIStyleRule.h"
#endif
#include "nsILayoutHistoryState.h"
#include "nsIStatefulFrame.h"
#include "nsIPresState.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIXMLContent.h"
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
#include "nsIContentList.h"
#include "nsReadableUtils.h"

#define NEW_CONTEXT_PARENTAGE_INVARIANT

#ifdef NEW_CONTEXT_PARENTAGE_INVARIANT

  #ifdef DEBUG
    #undef NOISY_DEBUG
  #else
    #undef NOISY_DEBUG
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

#endif // NEW_CONTEXT_PARENTAGE_INVARIANT

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// IID's

//----------------------------------------------------------------------

// Thin veneer around an NSPR hash table. Provides a map from a key that's
// a pointer to a value that's also a pointer.
class FrameHashTable {
public:
  FrameHashTable(PRUint32 aNumBuckets = 16);
  ~FrameHashTable();

  // Gets the value associated with the specified key
  void* Get(void* aKey);

  // Creates an association between the key and value. Returns the previous
  // value associated with the key, or NULL if there was none
  void* Put(void* aKey, void* aValue);

  // Removes association for the key, and returns its associated value
  void* Remove(void* aKey);

  // Removes all entries from the hash table
  void  Clear();

#ifdef NS_DEBUG
  void  Dump(FILE* fp);
#endif

protected:
  PLHashTable* mTable;
};

MOZ_DECL_CTOR_COUNTER(UndisplayedNode)

class UndisplayedNode {
public:
  UndisplayedNode(nsIContent* aContent, nsIStyleContext* aStyle)
  {
    MOZ_COUNT_CTOR(UndisplayedNode);
    mContent = aContent;
    mStyle = aStyle;
    NS_ADDREF(mStyle);
    mNext = nsnull;
  }
  UndisplayedNode(nsIStyleContext* aPseudoStyle) 
  {
    MOZ_COUNT_CTOR(UndisplayedNode);
    mContent = nsnull;
    mStyle = aPseudoStyle;
    NS_ADDREF(mStyle);
    mNext = nsnull;
  }
  ~UndisplayedNode(void)
  {
    MOZ_COUNT_DTOR(UndisplayedNode);
    NS_RELEASE(mStyle);
    if (mNext) {
      delete mNext;
    }
  }

  nsIContent*       mContent;
  nsIStyleContext*  mStyle;
  UndisplayedNode*  mNext;
};

class UndisplayedMap {
public:
  UndisplayedMap(PRUint32 aNumBuckets = 16);
  ~UndisplayedMap(void);

  UndisplayedNode* GetFirstNode(nsIContent* aParentContent);

  nsresult AddNodeFor(nsIContent* aParentContent, nsIContent* aChild, nsIStyleContext* aStyle);
  nsresult AddNodeFor(nsIContent* aParentContent, nsIStyleContext* aPseudoStyle);

  nsresult RemoveNodeFor(nsIContent* aParentContent, UndisplayedNode* aNode);
  nsresult RemoveNodesFor(nsIContent* aParentContent);

  nsresult GetNodeFor(nsIContent* aContent, nsIStyleContext** aResult);

  // Removes all entries from the hash table
  void  Clear(void);

protected:
  PLHashEntry** GetEntryFor(nsIContent* aParentContent);
  nsresult      AppendNodeFor(UndisplayedNode* aNode, nsIContent* aParentContent);

  PLHashTable*  mTable;
  PLHashEntry** mLastLookup;
};

//----------------------------------------------------------------------

class FrameManager;

  // A CantRenderReplacedElementEvent has a weak pointer to the frame
  // manager, and the frame manager has a weak pointer to the event.
  // The event queue owns the event and the FrameManager will delete
  // the event if it's going to go away.
struct CantRenderReplacedElementEvent : public PLEvent {
  CantRenderReplacedElementEvent(FrameManager* aFrameManager, nsIFrame* aFrame);

  nsIFrame*  mFrame;                     // the frame that can't be rendered
  CantRenderReplacedElementEvent* mNext; // next event in the list
};

class FrameManager : public nsIFrameManager
{
public:
  FrameManager();
  virtual ~FrameManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIFrameManager
  NS_IMETHOD Init(nsIPresShell* aPresShell, nsIStyleSet* aStyleSet);
  NS_IMETHOD Destroy();

  // Gets and sets the root frame
  NS_IMETHOD GetRootFrame(nsIFrame** aRootFrame) const;
  NS_IMETHOD SetRootFrame(nsIFrame* aRootFrame);
  
  // Get the canvas frame: searches from the Root frame down, may be null
  NS_IMETHOD GetCanvasFrame(nsIPresContext* aPresContext, nsIFrame** aCanvasFrame) const;

  // Primary frame functions
  NS_IMETHOD GetPrimaryFrameFor(nsIContent* aContent, nsIFrame** aPrimaryFrame);
  NS_IMETHOD SetPrimaryFrameFor(nsIContent* aContent,
                                nsIFrame*   aPrimaryFrame);
  NS_IMETHOD ClearPrimaryFrameMap();

  // Placeholder frame functions
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const;
  NS_IMETHOD SetPlaceholderFrameFor(nsIFrame* aFrame,
                                    nsIFrame* aPlaceholderFrame);
  NS_IMETHOD ClearPlaceholderFrameMap();

  // Undisplayed content functions
  NS_IMETHOD GetUndisplayedContent(nsIContent* aContent, nsIStyleContext** aStyleContext);
  NS_IMETHOD SetUndisplayedContent(nsIContent* aContent, nsIStyleContext* aStyleContext);
  NS_IMETHOD SetUndisplayedPseudoIn(nsIStyleContext* aPseudoContext, 
                                    nsIContent* aParentContent);
  NS_IMETHOD ClearUndisplayedContentIn(nsIContent* aContent, nsIContent* aParentContent);
  NS_IMETHOD ClearAllUndisplayedContentIn(nsIContent* aParentContent);
  NS_IMETHOD ClearUndisplayedContentMap();

  // Functions for manipulating the frame model
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIFrame*       aParentFrame,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);
  NS_IMETHOD ReplaceFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame,
                          nsIFrame*       aNewFrame);
  
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);

  NS_IMETHOD ReParentStyleContext(nsIPresContext* aPresContext,
                                  nsIFrame* aFrame, 
                                  nsIStyleContext* aNewParentContext);
  NS_IMETHOD ComputeStyleChangeFor(nsIPresContext* aPresContext,
                                   nsIFrame* aFrame, 
                                   PRInt32 aAttrNameSpaceID,
                                   nsIAtom* aAttribute,
                                   nsStyleChangeList& aChangeList,
                                   PRInt32 aMinChange,
                                   PRInt32& aTopLevelChange);
  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects);

  // Capture state from the entire frame heirarchy and store in aState
  NS_IMETHOD CaptureFrameState(nsIPresContext*        aPresContext,
                               nsIFrame*              aFrame,
                               nsILayoutHistoryState* aState);
  NS_IMETHOD RestoreFrameState(nsIPresContext*        aPresContext,
                               nsIFrame*              aFrame,
                               nsILayoutHistoryState* aState);
  // Add/restore state for one frame (special, global type, like scroll position)
  NS_IMETHOD CaptureFrameStateFor(nsIPresContext*     aPresContext,
                               nsIFrame*              aFrame,
                               nsILayoutHistoryState* aState,
                               nsIStatefulFrame::SpecialStateID aID = nsIStatefulFrame::eNoID);
  NS_IMETHOD RestoreFrameStateFor(nsIPresContext*     aPresContext,
                               nsIFrame*              aFrame,
                               nsILayoutHistoryState* aState,
                               nsIStatefulFrame::SpecialStateID aID = nsIStatefulFrame::eNoID);
  NS_IMETHOD GenerateStateKey(nsIContent* aContent,
                              nsIStatefulFrame::SpecialStateID aID,
                              nsCString& aString);

  // Gets and sets properties on a given frame
  NS_IMETHOD GetFrameProperty(nsIFrame* aFrame,
                              nsIAtom*  aPropertyName,
                              PRUint32  aOptions,
                              void**    aPropertyValue);
  NS_IMETHOD SetFrameProperty(nsIFrame*            aFrame,
                              nsIAtom*             aPropertyName,
                              void*                aPropertyValue,
                              NSFMPropertyDtorFunc aPropDtorFunc);
  NS_IMETHOD RemoveFrameProperty(nsIFrame* aFrame,
                                 nsIAtom*  aPropertyName);

#ifdef NS_DEBUG
  NS_IMETHOD DebugVerifyStyleTree(nsIPresContext* aPresContext, nsIFrame* aFrame);
#endif

private:
  struct PropertyList {
    nsCOMPtr<nsIAtom>    mName;          // property name
    nsDST*               mFrameValueMap; // map of frame/value pairs
    NSFMPropertyDtorFunc mDtorFunc;      // property specific value dtor function
    PropertyList*        mNext;

    PropertyList(nsIAtom*             aName,
                 NSFMPropertyDtorFunc aDtorFunc,
                 nsDST::NodeArena*    aDSTNodeArena);
    ~PropertyList();

    // Removes the property associated with the given frame, and destroys
    // the property value
    PRBool RemovePropertyForFrame(nsIPresContext* aPresContext, nsIFrame* aFrame);

    // Remove and destroy all remaining properties
    void   RemoveAllProperties(nsIPresContext* aPresContext);
  };

  nsIPresShell*                   mPresShell;    // weak link, because the pres shell owns us
  nsIStyleSet*                    mStyleSet;     // weak link. pres shell holds a reference
  nsIFrame*                       mRootFrame;
  nsDST::NodeArena*               mDSTNodeArena; // weak link. DST owns
  nsDST*                          mPrimaryFrameMap;
  FrameHashTable*                 mPlaceholderMap;
  UndisplayedMap*                 mUndisplayedMap;
  CantRenderReplacedElementEvent* mPostedEvents;
  PropertyList*                   mPropertyList;
  nsCOMPtr<nsIContentList>        mHTMLForms;
  nsCOMPtr<nsIContentList>        mHTMLFormControls;

  void ReResolveStyleContext(nsIPresContext* aPresContext,
                             nsIFrame* aFrame,
                             nsIStyleContext* aParentContext,
                             nsIContent* aParentContent,
                             PRInt32 aAttrNameSpaceID,
                             nsIAtom* aAttribute,
                             nsStyleChangeList& aChangeList, 
                             PRInt32 aMinChange,
                             PRInt32& aResultChange);

  nsresult RevokePostedEvents();
  CantRenderReplacedElementEvent** FindPostedEventFor(nsIFrame* aFrame);
  void DequeuePostedEventFor(nsIFrame* aFrame);
  void DestroyPropertyList(nsIPresContext* aPresContext);
  PropertyList* GetPropertyListFor(nsIAtom* aPropertyName) const;
  void RemoveAllPropertiesFor(nsIPresContext* aPresContext, nsIFrame* aFrame);

  friend struct CantRenderReplacedElementEvent;
  static void HandlePLEvent(CantRenderReplacedElementEvent* aEvent);
  static void DestroyPLEvent(CantRenderReplacedElementEvent* aEvent);
};

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewFrameManager(nsIFrameManager** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  if (!aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  FrameManager* it = new FrameManager;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIFrameManager), (void **)aInstancePtrResult);
}

FrameManager::FrameManager()
{
  NS_INIT_REFCNT();
}

FrameManager::~FrameManager()
{
  NS_ASSERTION(!mPresShell, "FrameManager::Destroy never called");
}

NS_IMPL_ISUPPORTS1(FrameManager, nsIFrameManager)

NS_IMETHODIMP
FrameManager::Init(nsIPresShell* aPresShell,
                   nsIStyleSet*  aStyleSet)
{
  NS_ASSERTION(aPresShell, "null aPresShell");
  NS_ASSERTION(aStyleSet, "null aStyleSet");

  mPresShell = aPresShell;
  mStyleSet = aStyleSet;

  // Allocate the node arena that's shared by all the DST objects
  mDSTNodeArena = nsDST::NewMemoryArena();
  if (!mDSTNodeArena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Force the forms and form control content lists to be added as
  // document observers *before* us (pres shell) so they will be
  // up to date when we try to use them.
  nsCOMPtr<nsIDocument> document;
  mPresShell->GetDocument(getter_AddRefs(document));
  nsCOMPtr<nsIHTMLDocument> htmlDocument(do_QueryInterface(document));
  nsCOMPtr<nsIDOMHTMLDocument> domHtmlDocument(do_QueryInterface(htmlDocument));
  if (domHtmlDocument) {
    nsCOMPtr<nsIDOMHTMLCollection> forms;
    domHtmlDocument->GetForms(getter_AddRefs(forms));
    mHTMLForms = do_QueryInterface(forms);

    nsCOMPtr<nsIDOMNodeList> formControls;
    htmlDocument->GetFormControlElements(getter_AddRefs(formControls));
    mHTMLFormControls = do_QueryInterface(formControls);
  }

  return NS_OK;
}

NS_IMETHODIMP
FrameManager::Destroy()
{
  NS_ASSERTION(mPresShell, "Frame manager already shut down.");

  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));
  
  // Destroy the frame hierarchy. Don't destroy the property lists until after
  // we've destroyed the frame hierarchy because some frames may expect to be
  // able to retrieve their properties during destruction
  if (mRootFrame) {
    mRootFrame->Destroy(presContext);
    mRootFrame = nsnull;
  }
  
  delete mPrimaryFrameMap;
  delete mPlaceholderMap;
  delete mUndisplayedMap;
  DestroyPropertyList(presContext);

  // If we're not going to be used anymore, we should revoke the
  // pending |CantRenderReplacedElementEvent|s being sent to us.
  nsresult rv = RevokePostedEvents();
  NS_ASSERTION(NS_SUCCEEDED(rv), "RevokePostedEvents failed:  might crash");

  mPresShell = nsnull; // mPresShell isn't valid anymore.  We
                       // won't use it, either, but we check it
                       // at the start of every function so that we'll
                       // be OK when nsIPresShell is converted to IDL.

  return rv;
}

NS_IMETHODIMP
FrameManager::GetRootFrame(nsIFrame** aRootFrame) const
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aRootFrame);
  *aRootFrame = mRootFrame;
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::SetRootFrame(nsIFrame* aRootFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(!mRootFrame, "already have a root frame");
  if (mRootFrame) {
    return NS_ERROR_UNEXPECTED;
  }

  mRootFrame = aRootFrame;
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::GetCanvasFrame(nsIPresContext* aPresContext, nsIFrame** aCanvasFrame) const
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(aCanvasFrame, "aCanvasFrame argument cannot be null");
  NS_PRECONDITION(aPresContext, "aPresContext argument cannot be null");

  *aCanvasFrame = nsnull;
  if (mRootFrame) {
    // walk the children of the root frame looking for a frame with type==canvas
    // start at the root
    nsIFrame* childFrame = mRootFrame;
    while (childFrame) {
      // get each sibling of the child and check them, startig at the child
      nsIFrame *siblingFrame = childFrame;
      while (siblingFrame) {
        nsCOMPtr<nsIAtom>  frameType;
        siblingFrame->GetFrameType(getter_AddRefs(frameType));
        if (frameType.get() == nsLayoutAtoms::canvasFrame) {
          // this is it: set the out-arg and stop looking
          *aCanvasFrame = siblingFrame;
          break;
        } else {
          siblingFrame->GetNextSibling(&siblingFrame);
        }
      }
      // move on to the child's child
      childFrame->FirstChild(aPresContext, nsnull, &childFrame);
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

// Primary frame functions
NS_IMETHODIMP
FrameManager::GetPrimaryFrameFor(nsIContent* aContent, nsIFrame** aResult)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aContent);
  if (!aContent || !aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;  // initialize out param

  nsresult rv;
  if (mPrimaryFrameMap) {
    mPrimaryFrameMap->Search(aContent, 0, (void**)aResult);
    if (!*aResult) {
      // XXX: todo:  Add a lookup into the undisplay map to skip searches 
      //             if we already know the content has no frame.
      //             nsCSSFrameConstructor calls SetUndisplayedContent() for every
      //             content node that has display: none.
      //             Today, the undisplay map doesn't quite support what we need.
      //             We need to see if we can add a method to make a search for aContent 
      //             very fast in the embedded hash table.
      //             This would almost completely remove the lookup penalty for things
      //             like <SCRIPT> and comments in very large documents.
      nsCOMPtr<nsIStyleSet>    styleSet;
      nsCOMPtr<nsIPresContext> presContext;

      // Give the frame construction code the opportunity to return the
      // frame that maps the content object
      mPresShell->GetStyleSet(getter_AddRefs(styleSet));
      NS_ASSERTION(styleSet, "bad style set");
      mPresShell->GetPresContext(getter_AddRefs(presContext));
      NS_ASSERTION(presContext, "bad presContext");
      if (!styleSet || !presContext) {
        return NS_ERROR_NULL_POINTER;
      }

      // if the prev sibling of aContent has a cached primary frame,
      // pass that data in to the style set to speed things up
      // if any methods in here fail, don't report that failure
      // we're just trying to enhance performance here, not test for correctness
      nsFindFrameHint hint;
      nsCOMPtr<nsIContent> prevSibling, parent;
      rv = aContent->GetParent(*getter_AddRefs(parent));
      if (NS_SUCCEEDED(rv) && parent)
      {
        PRInt32 index;
        rv = parent->IndexOf(aContent, index);
        if (NS_SUCCEEDED(rv) && index>0)  // no use looking if it's the first child
        {
          rv = parent->ChildAt(index-1, *getter_AddRefs(prevSibling));
          if (NS_SUCCEEDED(rv) && prevSibling)
          {
            mPrimaryFrameMap->Search(prevSibling.get(), 0, (void**)&hint.mPrimaryFrameForPrevSibling);
          }
        }
      }

      // walk the frame tree to find the frame that maps aContent.  
      // Use the hint if we have it.
      styleSet->FindPrimaryFrameFor(presContext, this, aContent, aResult, 
                                    hint.mPrimaryFrameForPrevSibling ? &hint : nsnull);
      
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::SetPrimaryFrameFor(nsIContent* aContent,
                                 nsIFrame*   aPrimaryFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aContent);
  // it's ok if aPrimaryFrame is null

  // If aPrimaryFrame is NULL, then remove the mapping
  if (!aPrimaryFrame) {
    if (mPrimaryFrameMap) {
      mPrimaryFrameMap->Remove(aContent);
    }
  } else {
    // Create a new DST if necessary
    if (!mPrimaryFrameMap) {
      mPrimaryFrameMap = new nsDST(mDSTNodeArena);
      if (!mPrimaryFrameMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    // Add a mapping to the hash table
    mPrimaryFrameMap->Insert(aContent, (void*)aPrimaryFrame, nsnull);
  }
    
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::ClearPrimaryFrameMap()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (mPrimaryFrameMap) {
    mPrimaryFrameMap->Clear();
  }
  return NS_OK;
}

// Placeholder frame functions
NS_IMETHODIMP
FrameManager::GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                     nsIFrame** aResult) const
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aFrame);
  if (!aResult || !aFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mPlaceholderMap) {
    *aResult = (nsIFrame*)mPlaceholderMap->Get(aFrame);
  } else {
    *aResult = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
FrameManager::SetPlaceholderFrameFor(nsIFrame* aFrame,
                                     nsIFrame* aPlaceholderFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aFrame);
#ifdef NS_DEBUG
  // Verify that the placeholder frame is of the correct type
  if (aPlaceholderFrame) {
    nsIAtom*  frameType;
  
    aPlaceholderFrame->GetFrameType(&frameType);
    NS_PRECONDITION(nsLayoutAtoms::placeholderFrame == frameType, "unexpected frame type");
    NS_IF_RELEASE(frameType);
  }
#endif

  // If aPlaceholderFrame is NULL, then remove the mapping
  if (!aPlaceholderFrame) {
    if (mPlaceholderMap) {
      mPlaceholderMap->Remove(aFrame);
    }
  } else {
    // Create a new hash table if necessary
    if (!mPlaceholderMap) {
      mPlaceholderMap = new FrameHashTable;
      if (!mPlaceholderMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    
    // Add a mapping to the hash table
    mPlaceholderMap->Put(aFrame, (void*)aPlaceholderFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::ClearPlaceholderFrameMap()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (mPlaceholderMap) {
    mPlaceholderMap->Clear();
  }
  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
FrameManager::GetUndisplayedContent(nsIContent* aContent, nsIStyleContext** aResult)
{
  if (!aContent || !aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;  // initialize out param

  if (mUndisplayedMap)
    mUndisplayedMap->GetNodeFor(aContent, aResult);
    
  return NS_OK;
}
  
NS_IMETHODIMP
FrameManager::SetUndisplayedContent(nsIContent* aContent, 
                                    nsIStyleContext* aStyleContext)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (! mUndisplayedMap) {
    mUndisplayedMap = new UndisplayedMap;
  }
  if (mUndisplayedMap) {
    nsresult result = NS_OK;
    nsIContent* parent = nsnull;
    aContent->GetParent(parent);
    if (parent) {
      result = mUndisplayedMap->AddNodeFor(parent, aContent, aStyleContext);
      NS_RELEASE(parent);
    }
    return result;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
FrameManager::SetUndisplayedPseudoIn(nsIStyleContext* aPseudoContext, 
                                     nsIContent* aParentContent)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (! mUndisplayedMap) {
    mUndisplayedMap = new UndisplayedMap;
  }
  if (mUndisplayedMap) {
    return mUndisplayedMap->AddNodeFor(aParentContent, aPseudoContext);
  }
  return NS_ERROR_OUT_OF_MEMORY;
}
                                     
NS_IMETHODIMP
FrameManager::ClearUndisplayedContentIn(nsIContent* aContent, nsIContent* aParentContent)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (mUndisplayedMap) {
    UndisplayedNode* node = mUndisplayedMap->GetFirstNode(aParentContent);
    while (node) {
      if (node->mContent == aContent) {
        return mUndisplayedMap->RemoveNodeFor(aParentContent, node);
      }
      node = node->mNext;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::ClearAllUndisplayedContentIn(nsIContent* aParentContent)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (mUndisplayedMap) {
    return mUndisplayedMap->RemoveNodesFor(aParentContent);
  }
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::ClearUndisplayedContentMap()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (mUndisplayedMap) {
    mUndisplayedMap->Clear();
  }
  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
FrameManager::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  return aParentFrame->AppendFrames(aPresContext, aPresShell, aListName,
                                    aFrameList);
}

NS_IMETHODIMP
FrameManager::InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
#ifdef IBMBIDI
  if (aPrevFrame) {
    // Insert aFrameList after the last bidi continuation of aPrevFrame.
    nsIFrame* nextBidi;
    for (; ;) {
      GetFrameProperty(aPrevFrame, nsLayoutAtoms::nextBidi, 0, (void**) &nextBidi);
      if (!nextBidi) {
        break;
      }
      aPrevFrame = nextBidi;
    }
  }
#endif // IBMBIDI

  return aParentFrame->InsertFrames(aPresContext, aPresShell, aListName,
                                    aPrevFrame, aFrameList);
}

NS_IMETHODIMP
FrameManager::RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);

#ifdef IBMBIDI
  // Don't let the parent remove next bidi. In the other cases the it should NOT be removed.
  nsIFrame* nextBidi;
  GetFrameProperty(aOldFrame, nsLayoutAtoms::nextBidi, 0, (void**) &nextBidi);
  if (nextBidi) {
    RemoveFrame(aPresContext, aPresShell, aParentFrame, aListName, nextBidi);
  }
#endif // IBMBIDI

  return aParentFrame->RemoveFrame(aPresContext, aPresShell, aListName,
                                   aOldFrame);
}

NS_IMETHODIMP
FrameManager::ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  return aParentFrame->ReplaceFrame(aPresContext, aPresShell, aListName,
                                    aOldFrame, aNewFrame);
}

//----------------------------------------------------------------------

NS_IMETHODIMP
FrameManager::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  // Dequeue and destroy and posted events for this frame
  DequeuePostedEventFor(aFrame);

  // Remove all properties associated with the frame
  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));

  RemoveAllPropertiesFor(presContext, aFrame);
  return NS_OK;
}

nsresult
FrameManager::RevokePostedEvents()
{
  nsresult rv = NS_OK;
#ifdef NOISY_EVENTS
  printf("%p ~RevokePostedEvents() start\n", this);
#endif
  if (mPostedEvents) {
    mPostedEvents = nsnull;

    // Revoke any events in the event queue that are owned by us
    nsIEventQueueService* eventService;

    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      NS_GET_IID(nsIEventQueueService),
                                      (nsISupports **)&eventService);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                             getter_AddRefs(eventQueue));
      nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

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
FrameManager::FindPostedEventFor(nsIFrame* aFrame)
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
FrameManager::DequeuePostedEventFor(nsIFrame* aFrame)
{
  // If there's a posted event for this frame, then remove it
  CantRenderReplacedElementEvent** event = FindPostedEventFor(aFrame);
  if (*event) {
    CantRenderReplacedElementEvent* tmp = *event;

    // Remove it from our linked list of posted events
    *event = (*event)->mNext;
    
    // Dequeue it from the event queue
    nsIEventQueueService* eventService;
    nsresult              rv;

    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      NS_GET_IID(nsIEventQueueService),
                                      (nsISupports **)&eventService);
    NS_ASSERTION(NS_SUCCEEDED(rv),
            "will crash soon due to event holding dangling pointer to frame");
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                             getter_AddRefs(eventQueue));
      nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

      NS_ASSERTION(NS_SUCCEEDED(rv) && eventQueue,
            "will crash soon due to event holding dangling pointer to frame");
      if (NS_SUCCEEDED(rv) && eventQueue) {
        PLEventQueue* plqueue;

        eventQueue->GetPLEventQueue(&plqueue);
        NS_ASSERTION(plqueue,
            "will crash soon due to event holding dangling pointer to frame");
        if (plqueue) {
          // Removes the event and destroys it
          PL_DequeueEvent(tmp, plqueue);
        }
      }
    }
  }
}

void
FrameManager::HandlePLEvent(CantRenderReplacedElementEvent* aEvent)
{
#ifdef NOISY_EVENTS
  printf("FrameManager::HandlePLEvent() start for FM %p\n", aEvent->owner);
#endif
  FrameManager* frameManager = (FrameManager*)aEvent->owner;
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
  nsCOMPtr<nsIPresContext> presContext;    
  frameManager->mPresShell->GetPresContext(getter_AddRefs(presContext));
  frameManager->mStyleSet->CantRenderReplacedElement(presContext,
                                                     aEvent->mFrame);        
#ifdef NOISY_EVENTS
  printf("FrameManager::HandlePLEvent() end for FM %p\n", aEvent->owner);
#endif
}

void
FrameManager::DestroyPLEvent(CantRenderReplacedElementEvent* aEvent)
{
  delete aEvent;
}

CantRenderReplacedElementEvent::CantRenderReplacedElementEvent(FrameManager* aFrameManager,
                                                               nsIFrame*     aFrame)
{
  PL_InitEvent(this, aFrameManager,
               (PLHandleEventProc)&FrameManager::HandlePLEvent,
               (PLDestroyEventProc)&FrameManager::DestroyPLEvent);
  mFrame = aFrame;
}

NS_IMETHODIMP
FrameManager::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                        nsIFrame*       aFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
#ifdef NOISY_EVENTS
  printf("%p FrameManager::CantRenderReplacedElement called\n", this);
#endif

  // We need to notify the style stystem, but post the notification so it
  // doesn't happen now
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> eventService = do_GetService(kEventQueueServiceCID, &rv);

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
      ev = new CantRenderReplacedElementEvent(this, aFrame);

      // Add the event to our linked list of posted events
      ev->mNext = mPostedEvents;
      mPostedEvents = ev;

      // Post the event
      eventQueue->PostEvent(ev);
    }
  }

  return rv;
}

#ifdef NS_DEBUG
static void
DumpContext(nsIFrame* aFrame, nsIStyleContext* aContext)
{
  if (aFrame) {
    fputs("frame: ", stdout);
    nsAutoString  name;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
      fputs(name, stdout);
    }
    fprintf(stdout, " (%p)", NS_STATIC_CAST(void*, aFrame));
  }
  if (aContext) {
    fprintf(stdout, " style: %p ", NS_STATIC_CAST(void*, aContext));

    nsIAtom* pseudoTag;
    aContext->GetPseudoType(pseudoTag);
    if (pseudoTag) {
      nsAutoString  buffer;
      pseudoTag->ToString(buffer);
      fputs(buffer, stdout);
      fputs(" ", stdout);
      NS_RELEASE(pseudoTag);
    }

/* XXXdwh fix debugging here.  Need to add a List method to nsIRuleNode
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

#ifdef NEW_CONTEXT_PARENTAGE_INVARIANT
static void
VerifyContextParent(nsIPresContext* aPresContext, nsIFrame* aFrame, 
                    nsIStyleContext* aContext, nsIStyleContext* aParentContext)
{
  nsIAtom*  frameType;

  // get the contexts not provided
  if (!aContext) {
    aFrame->GetStyleContext(&aContext);
  } else {
    // addref here so we can release at end
    NS_ADDREF(aContext);
  }

  if (!aParentContext) {
    // Get the correct parent context from the frame
    //  - if the frame is a placeholder, we get the out of flow frame's context 
    //    as the parent context instead of asking the frame
    aFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::placeholderFrame == frameType) { // placeholder
      // get out of flow frame's context as the parent context
      nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)aFrame)->GetOutOfFlowFrame();
      NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
      outOfFlowFrame->GetStyleContext(&aParentContext);
    } else {
      // get the parent context from the frame (indirectly)
      nsIFrame* providerFrame = nsnull;
      nsContextProviderRelationship relationship;
      aFrame->GetParentStyleContextProvider(aPresContext,&providerFrame,relationship);
      if (providerFrame) {
        providerFrame->GetStyleContext(&aParentContext);
      } else {
        // no parent context provider: it is OK, some frames' contexts do not have parents
      }
    }
    NS_IF_RELEASE(frameType);
  } else {
    // addref the parent context so we can release it at end
    NS_ADDREF(aParentContext);
  }

  NS_ASSERTION(aContext, "Failure to get required contexts");
  nsIStyleContext*  actualParentContext = aContext->GetParent();

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

  NS_IF_RELEASE(actualParentContext);
  NS_IF_RELEASE(aParentContext);
  NS_IF_RELEASE(aContext);
}

static void
VerifyStyleTree(nsIPresContext* aPresContext, nsIFrame* aFrame, nsIStyleContext* aParentContext)
{
  nsIStyleContext*  context;
  aFrame->GetStyleContext(&context);

  VerifyContextParent(aPresContext, aFrame, context, aParentContext);

  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;
  nsIFrame* child;
  nsIAtom*  frameType;

  do {
    child = nsnull;
    nsresult result = aFrame->FirstChild(aPresContext, childList, &child);
    while ((NS_SUCCEEDED(result)) && child) {
      nsFrameState  state;
      child->GetFrameState(&state);
      if (NS_FRAME_OUT_OF_FLOW != (state & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        child->GetFrameType(&frameType);
        if (nsLayoutAtoms::placeholderFrame == frameType) { 
          // placeholder: first recirse and verify the out of flow frame,
          // then verify the placeholder's context
          nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
          nsIStyleContext* outOfFlowContext;

          // recurse to out of flow frame, letting the parent context get resolved
          VerifyStyleTree(aPresContext, outOfFlowFrame, nsnull);

          // verify placeholder using the out of flow's context as parent context
          outOfFlowFrame->GetStyleContext(&outOfFlowContext);
          VerifyContextParent(aPresContext,child, nsnull, outOfFlowContext);
          NS_IF_RELEASE(outOfFlowContext);
        }
        else { // regular frame
          VerifyStyleTree(aPresContext, child, nsnull);
        }
        NS_IF_RELEASE(frameType);
      }
      child->GetNextSibling(&child);
    }

    NS_IF_RELEASE(childList);
    aFrame->GetAdditionalChildListName(listIndex++, &childList);
  } while (childList);
  
  // do additional contexts 
  PRInt32 contextIndex = -1;
  while (1 == 1) {
    nsIStyleContext* extraContext = nsnull;
    aFrame->GetAdditionalStyleContext(++contextIndex, &extraContext);
    if (extraContext) {
      VerifyContextParent(aPresContext, aFrame, extraContext, context);
      NS_RELEASE(extraContext);
    }
    else {
      break;
    }
  }
  NS_RELEASE(context);
}

NS_IMETHODIMP
FrameManager::DebugVerifyStyleTree(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (aFrame) {
    nsIStyleContext* context;
    aFrame->GetStyleContext(&context);
    nsIStyleContext* parentContext = context->GetParent();
    VerifyStyleTree(aPresContext, aFrame, parentContext);
    NS_IF_RELEASE(parentContext);
    NS_RELEASE(context);
  }
  return NS_OK;
}

#else  // NEW_CONTEXT_PARENTAGE_INVARIANT

static void
VerifyContextParent(nsIFrame* aFrame, nsIStyleContext* aContext, nsIStyleContext* aParentContext)
{
  nsIStyleContext*  actualParentContext = aContext->GetParent();

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
  NS_IF_RELEASE(actualParentContext);
}

static void
VerifyContextParent(nsIFrame* aFrame, nsIStyleContext* aParentContext)
{
  nsIStyleContext*  context;
  aFrame->GetStyleContext(&context);
  VerifyContextParent(aFrame, context, aParentContext);
  NS_RELEASE(context);
}

static void
VerifyStyleTree(nsIPresContext* aPresContext, nsIFrame* aFrame, nsIStyleContext* aParentContext)
{
  nsIStyleContext*  context;
  aFrame->GetStyleContext(&context);

  VerifyContextParent(aFrame, aParentContext);

  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;
  nsIFrame* child;
  nsIAtom*  frameType;

  do {
    child = nsnull;
    nsresult result = aFrame->FirstChild(aPresContext, childList, &child);
    while ((NS_SUCCEEDED(result)) && child) {
      nsFrameState  state;
      child->GetFrameState(&state);
      if (NS_FRAME_OUT_OF_FLOW != (state & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        child->GetFrameType(&frameType);
        if (nsLayoutAtoms::placeholderFrame == frameType) { // placeholder
          // get out of flow frame and recurse there
          nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

          nsIStyleContext* outOfFlowContext;
          outOfFlowFrame->GetStyleContext(&outOfFlowContext);
          VerifyContextParent(child, outOfFlowContext);
          NS_RELEASE(outOfFlowContext);

          VerifyStyleTree(aPresContext, outOfFlowFrame, context);
        }
        else { // regular frame
          VerifyStyleTree(aPresContext, child, context);
        }
        NS_IF_RELEASE(frameType);
      }

      child->GetNextSibling(&child);
    }

    NS_IF_RELEASE(childList);
    aFrame->GetAdditionalChildListName(listIndex++, &childList);
  } while (childList);
  
  // do additional contexts 
  PRInt32 contextIndex = -1;
  while (1 == 1) {
    nsIStyleContext* extraContext = nsnull;
    aFrame->GetAdditionalStyleContext(++contextIndex, &extraContext);
    if (extraContext) {
      VerifyContextParent(aFrame, extraContext, context);
      NS_RELEASE(extraContext);
    }
    else {
      break;
    }
  }
  NS_RELEASE(context);
}

NS_IMETHODIMP
FrameManager::DebugVerifyStyleTree(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  if (aFrame) {
    nsIStyleContext* context;
    aFrame->GetStyleContext(&context);
    nsIStyleContext* parentContext = context->GetParent();
    VerifyStyleTree(aPresContext, aFrame, parentContext);
    NS_IF_RELEASE(parentContext);
    NS_RELEASE(context);
  }
  return NS_OK;
}
#endif //NEW_CONTEXT_PARENTAGE_INVARIANT
#endif // DEBUG

NS_IMETHODIMP
FrameManager::ReParentStyleContext(nsIPresContext* aPresContext,
                                   nsIFrame* aFrame, 
                                   nsIStyleContext* aNewParentContext)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aFrame) {
#ifdef NS_DEBUG
    DebugVerifyStyleTree(aPresContext, aFrame);
#endif

    nsIStyleContext* oldContext = nsnull;
    aFrame->GetStyleContext(&oldContext);

    if (oldContext) {
      nsIStyleContext* newContext = nsnull;
      result = mStyleSet->ReParentStyleContext(aPresContext, 
                                               oldContext, aNewParentContext,
                                               &newContext);
      if (NS_SUCCEEDED(result) && newContext) {
        if (newContext != oldContext) {
          PRInt32 listIndex = 0;
          nsIAtom* childList = nsnull;
          nsIFrame* child;
          nsIAtom*  frameType;

          do {
            child = nsnull;
            result = aFrame->FirstChild(aPresContext, childList, &child);
            while ((NS_SUCCEEDED(result)) && child) {
              nsFrameState  state;
              child->GetFrameState(&state);
              if (NS_FRAME_OUT_OF_FLOW != (state & NS_FRAME_OUT_OF_FLOW)) {
                // only do frames that are in flow
                child->GetFrameType(&frameType);
                if (nsLayoutAtoms::placeholderFrame == frameType) { // placeholder
                  // get out of flow frame and recurse there
                  nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
                  NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

                  result = ReParentStyleContext(aPresContext, outOfFlowFrame, newContext);

                  // reparent placeholder's context under out of flow frame
                  nsIStyleContext*  outOfFlowContext;
                  outOfFlowFrame->GetStyleContext(&outOfFlowContext);
                  ReParentStyleContext(aPresContext, child, outOfFlowContext);
                  NS_RELEASE(outOfFlowContext);
                }
                else { // regular frame
                  result = ReParentStyleContext(aPresContext, child, newContext);
                }
                NS_IF_RELEASE(frameType);
              }

              child->GetNextSibling(&child);
            }

            NS_IF_RELEASE(childList);
            aFrame->GetAdditionalChildListName(listIndex++, &childList);
          } while (childList);
          
          aFrame->SetStyleContext(aPresContext, newContext);

          // do additional contexts 
          PRInt32 contextIndex = -1;
          while (1 == 1) {
            nsIStyleContext* oldExtraContext = nsnull;
            result = aFrame->GetAdditionalStyleContext(++contextIndex, &oldExtraContext);
            if (NS_SUCCEEDED(result)) {
              if (oldExtraContext) {
                nsIStyleContext* newExtraContext = nsnull;
                result = mStyleSet->ReParentStyleContext(aPresContext,
                                                         oldExtraContext, newContext,
                                                         &newExtraContext);
                if (NS_SUCCEEDED(result) && newExtraContext) {
                  aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
                  NS_RELEASE(newExtraContext);
                }
                NS_RELEASE(oldExtraContext);
              }
            }
            else {
              result = NS_OK; // ok not to have extras (or run out)
              break;
            }
          }
#ifdef NS_DEBUG
          VerifyStyleTree(aPresContext, aFrame, aNewParentContext);
#endif
        }
        NS_RELEASE(newContext);
      }
      NS_RELEASE(oldContext);
    }
  }
  return result;
}

static PRBool
HasAttributeContent(nsIStyleContext* aStyleContext, 
                    PRInt32 aNameSpaceID,
                    nsIAtom* aAttribute)
{
  PRBool  result = PR_FALSE;
  if (aStyleContext) {
    const nsStyleContent* content = (const nsStyleContent*)aStyleContext->GetStyleData(eStyleStruct_Content);
    PRUint32 count = content->ContentCount();
    while ((0 < count) && (! result)) {
      nsStyleContentType  contentType;
      nsAutoString        contentString;
      content->GetContentAt(--count, contentType, contentString);
      if (eStyleContentType_Attr == contentType) {
        nsIAtom* attrName = nsnull;
        PRInt32 attrNameSpace = kNameSpaceID_None;
        PRInt32 barIndex = contentString.FindChar('|'); // CSS namespace delimiter
        if (-1 != barIndex) {
          nsAutoString  nameSpaceVal;
          contentString.Left(nameSpaceVal, barIndex);
          PRInt32 error;
          attrNameSpace = nameSpaceVal.ToInteger(&error, 10);
          contentString.Cut(0, barIndex + 1);
          if (contentString.Length()) {
            attrName = NS_NewAtom(contentString);
          }
        }
        else {
          attrName = NS_NewAtom(contentString);
        }
        if ((attrName == aAttribute) && 
            ((attrNameSpace == aNameSpaceID) || 
             (attrNameSpace == kNameSpaceID_Unknown))) {
          result = PR_TRUE;
        }
        NS_IF_RELEASE(attrName);
      }
    }
  }
  return result;
}
    

static PRInt32
CaptureChange(nsIStyleContext* aOldContext, nsIStyleContext* aNewContext,
              nsIFrame* aFrame, nsIContent* aContent,
              nsStyleChangeList& aChangeList, PRInt32 aMinChange)
{
  PRInt32 ourChange = NS_STYLE_HINT_NONE;
  aNewContext->CalcStyleDifference(aOldContext, ourChange);
  if (aMinChange < ourChange) {
    aChangeList.AppendChange(aFrame, aContent, ourChange);
    aMinChange = ourChange;
  }
  return aMinChange;
}

#ifdef NEW_CONTEXT_PARENTAGE_INVARIANT

void
FrameManager::ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIFrame* aFrame,
                                    nsIStyleContext* aParentContext,
                                    nsIContent* aParentContent,
                                    PRInt32 aAttrNameSpaceID,
                                    nsIAtom* aAttribute,
                                    nsStyleChangeList& aChangeList, 
                                    PRInt32 aMinChange,
                                    PRInt32& aResultChange)
{
  nsIFrame *resolvedDescendant = nsnull;

  nsIStyleContext* oldContext = nsnull; 
  nsresult result = aFrame->GetStyleContext(&oldContext);
  if (NS_SUCCEEDED(result) && oldContext) {
    nsIAtom* pseudoTag = nsnull;
    oldContext->GetPseudoType(pseudoTag);
    nsIContent* localContent = nsnull;
    nsIContent* content = nsnull;
    result = aFrame->GetContent(&localContent);
    if (NS_SUCCEEDED(result) && localContent) {
      content = localContent;
    }
    else {
      content = aParentContent;
    }
    if (aParentContent && aAttribute) { // attribute came from parent, we don't care about it here when recursing
      nsFrameState  frameState;
      aFrame->GetFrameState(&frameState);
      if (0 == (frameState & NS_FRAME_GENERATED_CONTENT)) { // keep it for generated content
        aAttribute = nsnull;
      }
    }

    if (aParentContext == nsnull) {
      NOISY_TRACE_FRAME("null parent context in ReResolveStyle: getting provider frame for",aFrame);
      // get the parent context from the frame (indirectly)
      nsIFrame* providerFrame = nsnull;
      nsContextProviderRelationship relationship;
      aFrame->GetParentStyleContextProvider(aPresContext,&providerFrame,relationship);
      if (providerFrame) {
        NOISY_TRACE("provider frame retrieved in ReResolveStyle: ");

        // see if we need to recurse and resolve the provider frame first
        if (relationship != eContextProvider_Ancestor) {
          NOISY_TRACE("non-ancestor provider, recursing.\n");
          // provider is not an ancestor, so assume we have to reresolve it first
          ReResolveStyleContext(aPresContext, providerFrame, nsnull, content,
                                aAttrNameSpaceID, aAttribute,
                                aChangeList, aMinChange, aResultChange);
          resolvedDescendant = providerFrame;
          NOISY_TRACE("returned from recursion, descendant parent context provider resolved.\n");
        } else {
          NOISY_TRACE("ancestor provider, assuming already resolved.\n");
        }
        providerFrame->GetStyleContext(&aParentContext);
      }
    } else {
      // addref the parent passed in so we can release it at the end
      NS_ADDREF(aParentContext);
      NOISY_TRACE_FRAME("non-null parent context provided: using it and assuming already resolved",aFrame);
    }
    NS_ASSERTION(aParentContext, "Failed to resolve parent context");

    // do primary context
    nsIStyleContext* newContext = nsnull;
    if (pseudoTag) {
       nsIContent* pseudoContent = (aParentContent ? aParentContent : localContent);
       aPresContext->ResolvePseudoStyleContextFor(pseudoContent, pseudoTag, aParentContext, PR_FALSE,
                                                &newContext);
      NS_RELEASE(pseudoTag);
    }
    else {
      NS_ASSERTION(localContent, "non pseudo-element frame without content node");
      aPresContext->ResolveStyleContextFor(content, aParentContext, PR_TRUE, &newContext);
    }
    NS_ASSERTION(newContext, "failed to get new style context");
    if (newContext) {
      if (newContext != oldContext) {
        aMinChange = CaptureChange(oldContext, newContext, aFrame, content, aChangeList, aMinChange);
        if (aMinChange < NS_STYLE_HINT_FRAMECHANGE) { // if frame gets regenerated, let it keep old context
          aFrame->SetStyleContext(aPresContext, newContext);
        }
        // if old context had image and new context does not have the same image, 
        // stop the image load for the frame
        const nsStyleBackground* oldColor = (const nsStyleBackground*)oldContext->GetStyleData(eStyleStruct_Background); 
        const nsStyleBackground* newColor = (const nsStyleBackground*)newContext->GetStyleData(eStyleStruct_Background);

        if(oldColor->mBackgroundImage.Length() > 0 &&
          oldColor->mBackgroundImage != newColor->mBackgroundImage ){
          // stop the image loading for the frame, the image has changed
          aPresContext->StopImagesFor(aFrame);
        }
      }
      else {
        // XXXdwh figure this out.
        // oldContext->RemapStyle(aPresContext, PR_FALSE);
        if (aAttribute && (aMinChange < NS_STYLE_HINT_REFLOW) &&
            HasAttributeContent(oldContext, aAttrNameSpaceID, aAttribute)) {
          aChangeList.AppendChange(aFrame, content, NS_STYLE_HINT_REFLOW);
        }
      }
      NS_RELEASE(oldContext);
    }
    else {
      NS_ERROR("resolve style context failed");
      newContext = oldContext;  // new context failed, recover... (take ref)
    }

    // do additional contexts 
    PRInt32 contextIndex = -1;
    while (1 == 1) {
      nsIStyleContext* oldExtraContext = nsnull;
      result = aFrame->GetAdditionalStyleContext(++contextIndex, &oldExtraContext);
      if (NS_SUCCEEDED(result)) {
        if (oldExtraContext) {
          nsIStyleContext* newExtraContext = nsnull;
          oldExtraContext->GetPseudoType(pseudoTag);
          NS_ASSERTION(pseudoTag, "extra style context is not pseudo element");
          result = aPresContext->ResolvePseudoStyleContextFor(content, pseudoTag, newContext,
                                                             PR_FALSE, &newExtraContext);
          NS_RELEASE(pseudoTag);
          if (NS_SUCCEEDED(result) && newExtraContext) {
            if (oldExtraContext != newExtraContext) {
              aMinChange = CaptureChange(oldExtraContext, newExtraContext, aFrame, 
                                         content, aChangeList, aMinChange);
              if (aMinChange < NS_STYLE_HINT_FRAMECHANGE) {
                aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
              }
            }
            else {
              // XXXdwh figure this out.
              // oldExtraContext->RemapStyle(aPresContext, PR_FALSE);
              if (aAttribute && (aMinChange < NS_STYLE_HINT_REFLOW) &&
                  HasAttributeContent(oldContext, aAttrNameSpaceID, aAttribute)) {
                aChangeList.AppendChange(aFrame, content, NS_STYLE_HINT_REFLOW);
              }
            }
            NS_RELEASE(newExtraContext);
          }
          NS_RELEASE(oldExtraContext);
        }
      }
      else {
        break;
      }
    }

    // now look for undisplayed child content and pseudos
    if (localContent && mUndisplayedMap) {
      UndisplayedNode* undisplayed = mUndisplayedMap->GetFirstNode(localContent);
      while (undisplayed) {
        nsIStyleContext* undisplayedContext = nsnull;
        undisplayed->mStyle->GetPseudoType(pseudoTag);
        if (undisplayed->mContent && pseudoTag == nsnull) {  // child content
          aPresContext->ResolveStyleContextFor(undisplayed->mContent, newContext, 
                                              PR_TRUE, &undisplayedContext);
        }
        else {  // pseudo element
          NS_ASSERTION(pseudoTag, "pseudo element without tag");
          aPresContext->ResolvePseudoStyleContextFor(localContent, pseudoTag, newContext, PR_FALSE,
                                                    &undisplayedContext);
        }
        NS_IF_RELEASE(pseudoTag);
        if (undisplayedContext) {
          if (undisplayedContext == undisplayed->mStyle) {
            // XXXdwh figure this out.
            // undisplayedContext->RemapStyle(aPresContext);
          }
          const nsStyleDisplay* display = 
                (const nsStyleDisplay*)undisplayedContext->GetStyleData(eStyleStruct_Display);
          if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
            aChangeList.AppendChange(nsnull, ((undisplayed->mContent) ? undisplayed->mContent : localContent), 
                                     NS_STYLE_HINT_FRAMECHANGE);
          }
          NS_RELEASE(undisplayedContext);
        }
        undisplayed = undisplayed->mNext;
      }
    }

    aResultChange = aMinChange;

    // now do children
    PRInt32 listIndex = 0;
    nsIAtom* childList = nsnull;
    PRInt32 childChange;
    nsIFrame* child;

    do {
      child = nsnull;
      result = aFrame->FirstChild(aPresContext, childList, &child);
      while ((NS_SUCCEEDED(result)) && (child)) {
        nsFrameState  state;
        child->GetFrameState(&state);
        if (NS_FRAME_OUT_OF_FLOW != (state & NS_FRAME_OUT_OF_FLOW)) {
          // only do frames that are in flow
          nsCOMPtr<nsIAtom> frameType;
          child->GetFrameType(getter_AddRefs(frameType));
          if (nsLayoutAtoms::placeholderFrame == frameType.get()) { // placeholder
            // get out of flow frame and recurse there
            nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
            NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

            if (outOfFlowFrame != resolvedDescendant) {
              ReResolveStyleContext(aPresContext, outOfFlowFrame, nsnull, content,
                                    aAttrNameSpaceID, aAttribute,
                                    aChangeList, aMinChange, childChange);
            } else {
              NOISY_TRACE("out of flow frame already resolved as descendent\n");
            }

            // reresolve placeholder's context under out of flow frame
            nsIStyleContext*  outOfFlowContext;
            outOfFlowFrame->GetStyleContext(&outOfFlowContext);
            ReResolveStyleContext(aPresContext, child, outOfFlowContext, content,
                                  kNameSpaceID_Unknown, nsnull,
                                  aChangeList, aMinChange, childChange);
            NS_RELEASE(outOfFlowContext);
          }
          else {  // regular child frame
            if (child != resolvedDescendant) {
              ReResolveStyleContext(aPresContext, child, nsnull, content,
                                    aAttrNameSpaceID, aAttribute,
                                    aChangeList, aMinChange, childChange);
            } else {
              NOISY_TRACE_FRAME("child frame already resolved as descendent, skipping",aFrame);
            }
          }
        }
        child->GetNextSibling(&child);
      }

      NS_IF_RELEASE(childList);
      aFrame->GetAdditionalChildListName(listIndex++, &childList);
    } while (childList);
    // XXX need to do overflow frames???

    NS_RELEASE(newContext);
    NS_IF_RELEASE(localContent);
    NS_IF_RELEASE(aParentContext);
  }
}

#else // NEW_CONTEXT_PARENTAGE_INVARIANT

void
FrameManager::ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIFrame* aFrame,
                                    nsIStyleContext* aParentContext,
                                    nsIContent* aParentContent,
                                    PRInt32 aAttrNameSpaceID,
                                    nsIAtom* aAttribute,
                                    nsStyleChangeList& aChangeList, 
                                    PRInt32 aMinChange,
                                    PRInt32& aResultChange)
{
  nsIStyleContext* oldContext = nsnull; 
  nsresult result = aFrame->GetStyleContext(&oldContext);
  if (NS_SUCCEEDED(result) && oldContext) {
    nsIAtom* pseudoTag = nsnull;
    oldContext->GetPseudoType(pseudoTag);
    nsIContent* localContent = nsnull;
    nsIContent* content = nsnull;
    result = aFrame->GetContent(&localContent);
    if (NS_SUCCEEDED(result) && localContent) {
      content = localContent;
    }
    else {
      content = aParentContent;
    }
    if (aParentContent && aAttribute) { // attribute came from parent, we don't care about it here when recursing
      nsFrameState  frameState;
      aFrame->GetFrameState(&frameState);
      if (0 == (frameState & NS_FRAME_GENERATED_CONTENT)) { // keep it for generated content
        aAttribute = nsnull;
      }
    }

    // do primary context
    nsIStyleContext* newContext = nsnull;
    if (pseudoTag) {
      aPresContext->ResolvePseudoStyleContextFor(aParentContent, pseudoTag, aParentContext, PR_FALSE,
                                                &newContext);
      NS_RELEASE(pseudoTag);
    }
    else {
      NS_ASSERTION(localContent, "non pseudo-element frame without content node");
      aPresContext->ResolveStyleContextFor(content, aParentContext, PR_TRUE, &newContext);
    }
    NS_ASSERTION(newContext, "failed to get new style context");
    if (newContext) {
      if (newContext != oldContext) {
        aMinChange = CaptureChange(oldContext, newContext, aFrame, content, aChangeList, aMinChange);
        if (aMinChange < NS_STYLE_HINT_FRAMECHANGE) { // if frame gets regenerated, let it keep old context
          aFrame->SetStyleContext(aPresContext, newContext);
        }
        // if old context had image and new context does not have the same image, 
        // stop the image load for the frame
        nsStyleColor* oldColor;
        nsStyleColor* newColor;
        oldContext->GetStyle(eStyleStruct_Color, &oldColor);
        newContext->GetStyle(eStyleStruct_Color, &newColor);
        if(oldColor.mBackgroundImage.Length() > 0 &&
          oldColor.mBackgroundImage != newColor.mBackgroundImage ){
          // stop the image loading for the frame, the image has changed
          aPresContext->StopImagesFor(aFrame);
        }
      }
      else {
        // XXXdwh figure this out.
        // oldContext->RemapStyle(aPresContext, PR_FALSE);
        if (aAttribute && (aMinChange < NS_STYLE_HINT_REFLOW) &&
            HasAttributeContent(oldContext, aAttrNameSpaceID, aAttribute)) {
          aChangeList.AppendChange(aFrame, content, NS_STYLE_HINT_REFLOW);
        }
      }
      NS_RELEASE(oldContext);
    }
    else {
      NS_ERROR("resolve style context failed");
      newContext = oldContext;  // new context failed, recover... (take ref)
    }

    // do additional contexts 
    PRInt32 contextIndex = -1;
    while (1 == 1) {
      nsIStyleContext* oldExtraContext = nsnull;
      result = aFrame->GetAdditionalStyleContext(++contextIndex, &oldExtraContext);
      if (NS_SUCCEEDED(result)) {
        if (oldExtraContext) {
          nsIStyleContext* newExtraContext = nsnull;
          oldExtraContext->GetPseudoType(pseudoTag);
          NS_ASSERTION(pseudoTag, "extra style context is not pseudo element");
          result = aPresContext->ResolvePseudoStyleContextFor(content, pseudoTag, newContext,
                                                             PR_FALSE, &newExtraContext);
          NS_RELEASE(pseudoTag);
          if (NS_SUCCEEDED(result) && newExtraContext) {
            if (oldExtraContext != newExtraContext) {
              aMinChange = CaptureChange(oldExtraContext, newExtraContext, aFrame, 
                                         content, aChangeList, aMinChange);
              if (aMinChange < NS_STYLE_HINT_FRAMECHANGE) {
                aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
              }
            }
            else {
              // XXXdwh figure this out.
              // oldExtraContext->RemapStyle(aPresContext, PR_FALSE);
              if (aAttribute && (aMinChange < NS_STYLE_HINT_REFLOW) &&
                  HasAttributeContent(oldContext, aAttrNameSpaceID, aAttribute)) {
                aChangeList.AppendChange(aFrame, content, NS_STYLE_HINT_REFLOW);
              }
            }
            NS_RELEASE(newExtraContext);
          }
          NS_RELEASE(oldExtraContext);
        }
      }
      else {
        break;
      }
    }

    // now look for undisplayed child content and pseudos
    if (localContent && mUndisplayedMap) {
      UndisplayedNode* undisplayed = mUndisplayedMap->GetFirstNode(localContent);
      while (undisplayed) {
        nsIStyleContext* undisplayedContext = nsnull;
        undisplayed->mStyle->GetPseudoType(pseudoTag);
        if (undisplayed->mContent && pseudoTag == nsnull) {  // child content
          aPresContext->ResolveStyleContextFor(undisplayed->mContent, newContext, 
                                              PR_TRUE, &undisplayedContext);
        }
        else {  // pseudo element
          NS_ASSERTION(pseudoTag, "pseudo element without tag");
          aPresContext->ResolvePseudoStyleContextFor(localContent, pseudoTag, newContext, PR_FALSE,
                                                    &undisplayedContext);
        }
        NS_IF_RELEASE(pseudoTag);
        if (undisplayedContext) {
          if (undisplayedContext == undisplayed->mStyle) {
            // XXXdwh figure this out.
            // undisplayedContext->RemapStyle(aPresContext);
          }
          const nsStyleDisplay* display = 
                (const nsStyleDisplay*)undisplayedContext->GetStyleData(eStyleStruct_Display);
          if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
            aChangeList.AppendChange(nsnull, ((undisplayed->mContent) ? undisplayed->mContent : localContent), 
                                     NS_STYLE_HINT_FRAMECHANGE);
          }
          NS_RELEASE(undisplayedContext);
        }
        undisplayed = undisplayed->mNext;
      }
    }

    aResultChange = aMinChange;

    // now do children
    PRInt32 listIndex = 0;
    nsIAtom* childList = nsnull;
    PRInt32 childChange;
    nsIFrame* child;

    do {
      child = nsnull;
      result = aFrame->FirstChild(aPresContext, childList, &child);
      while ((NS_SUCCEEDED(result)) && (child)) {
        nsFrameState  state;
        child->GetFrameState(&state);
        if (NS_FRAME_OUT_OF_FLOW != (state & NS_FRAME_OUT_OF_FLOW)) {
          // only do frames that are in flow
          nsCOMPtr<nsIAtom> frameType;
          child->GetFrameType(getter_AddRefs(frameType));
          if (nsLayoutAtoms::placeholderFrame == frameType.get()) { // placeholder
            // get out of flow frame and recurse there
            nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
            NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

            ReResolveStyleContext(aPresContext, outOfFlowFrame, newContext, content,
                                  aAttrNameSpaceID, aAttribute,
                                  aChangeList, aMinChange, childChange);

            // reresolve placeholder's context under out of flow frame
            nsIStyleContext*  outOfFlowContext;
            outOfFlowFrame->GetStyleContext(&outOfFlowContext);
            ReResolveStyleContext(aPresContext, child, outOfFlowContext, content,
                                  kNameSpaceID_Unknown, nsnull,
                                  aChangeList, aMinChange, childChange);
            NS_RELEASE(outOfFlowContext);
          }
          else {  // regular child frame
            ReResolveStyleContext(aPresContext, child, newContext, content,
                                  aAttrNameSpaceID, aAttribute,
                                  aChangeList, aMinChange, childChange);
          }
        }
        child->GetNextSibling(&child);
      }

      NS_IF_RELEASE(childList);
      aFrame->GetAdditionalChildListName(listIndex++, &childList);
    } while (childList);
    // XXX need to do overflow frames???

    NS_RELEASE(newContext);
    NS_IF_RELEASE(localContent);
  }
}

#endif // NEW_CONTEXT_PARENTAGE_INVARIANT

NS_IMETHODIMP
FrameManager::ComputeStyleChangeFor(nsIPresContext* aPresContext,
                                    nsIFrame* aFrame, 
                                    PRInt32 aAttrNameSpaceID,
                                    nsIAtom* aAttribute,
                                    nsStyleChangeList& aChangeList,
                                    PRInt32 aMinChange,
                                    PRInt32& aTopLevelChange)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  aTopLevelChange = NS_STYLE_HINT_NONE;
  nsIFrame* frame = aFrame;

  do {
    nsIStyleContext* styleContext = nsnull;
    frame->GetStyleContext(&styleContext);
    nsIStyleContext* parentContext = styleContext->GetParent();
    PRInt32 frameChange;
    ReResolveStyleContext(aPresContext, frame, parentContext, nsnull,
                          aAttrNameSpaceID, aAttribute,
                          aChangeList, aMinChange, frameChange);
#ifdef NS_DEBUG
    VerifyStyleTree(aPresContext, frame, parentContext);
#endif
    NS_IF_RELEASE(parentContext);
    NS_RELEASE(styleContext);
    if (aTopLevelChange < frameChange) {
      aTopLevelChange = frameChange;
    }

    if (aTopLevelChange >= NS_STYLE_HINT_REFLOW) {
      // If it's going to cause a reflow (or worse), then don't touch
      // the any continuations: they must be taken care of by the reflow.
#ifdef NS_DEBUG
      nsIFrame* prevInFlow;
      frame->GetPrevInFlow(&prevInFlow);
      NS_ASSERTION(!prevInFlow, "continuing frame had more severe impact than first-in-flow");
#endif
      break;
    }

    frame->GetNextInFlow(&frame);
  } while (frame);
  return NS_OK;
}


NS_IMETHODIMP
FrameManager::AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                    PRBool &aAffects)
{
  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIXMLContent> xml(do_QueryInterface(aContent));
  if (xml) {
    rv = mStyleSet->AttributeAffectsStyle(aAttribute, aContent, aAffects);
  } else {
    // not an XML element, so assume it is an HTML element and further assume that
    // any attribute may affect style
    // NOTE: we could list all of the presentation-hint attributes and check them,
    //       but there are a lot of them and this is safer.
    aAffects = PR_TRUE;
    rv = NS_OK;
  }
  return rv;
}

// Capture state for a given frame.
// Accept a content id here, in some cases we may not have content (scroll position)
NS_IMETHODIMP
FrameManager::CaptureFrameStateFor(nsIPresContext* aPresContext,
                                   nsIFrame* aFrame,
                                   nsILayoutHistoryState* aState,
                                   nsIStatefulFrame::SpecialStateID aID)
{
  NS_ENSURE_TRUE(mPresShell && aFrame && aState, NS_ERROR_FAILURE);

  // Only capture state for stateful frames
  nsIStatefulFrame* statefulFrame = nsnull;
  aFrame->QueryInterface(NS_GET_IID(nsIStatefulFrame), (void**) &statefulFrame);
  if (!statefulFrame) {
    return NS_OK;
  }

  // Capture the state, exit early if we get null (nothing to save)
  nsCOMPtr<nsIPresState> frameState;
  nsresult rv = NS_OK;
  rv = statefulFrame->SaveState(aPresContext, getter_AddRefs(frameState));
  if (!frameState) {
    return NS_OK;
  }

  // Generate the hash key to store the state under
  // Exit early if we get empty key
  nsCOMPtr<nsIContent> content;
  rv = aFrame->GetContent(getter_AddRefs(content));

  nsCAutoString stateKey;
  rv = GenerateStateKey(content, aID, stateKey);
  if(NS_FAILED(rv) || stateKey.IsEmpty()) {
    return rv;
  }

  // Store the state
  return aState->AddState(stateKey, frameState);
}

NS_IMETHODIMP
FrameManager::CaptureFrameState(nsIPresContext* aPresContext,
                                nsIFrame* aFrame,
                                nsILayoutHistoryState* aState)
{
  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(nsnull != aFrame && nsnull != aState, "null parameters passed in");

  rv = CaptureFrameStateFor(aPresContext, aFrame, aState);

  // Now capture state recursively for the frame hierarchy rooted at aFrame
  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;
  do {    
    nsIFrame* childFrame;
    aFrame->FirstChild(aPresContext, childListName, &childFrame);
    while (childFrame) {             
      rv = CaptureFrameState(aPresContext, childFrame, aState);
      // Get the next sibling child frame
      childFrame->GetNextSibling(&childFrame);
    }
    NS_IF_RELEASE(childListName);
    aFrame->GetAdditionalChildListName(childListIndex++, &childListName);
  } while (childListName);

  return rv;
}

// Restore state for a given frame.
// Accept a content id here, in some cases we may not have content (scroll position)
NS_IMETHODIMP
FrameManager::RestoreFrameStateFor(nsIPresContext* aPresContext, nsIFrame* aFrame, nsILayoutHistoryState* aState, nsIStatefulFrame::SpecialStateID aID)
{
  NS_ENSURE_TRUE(mPresShell && aFrame && aState, NS_ERROR_FAILURE);

  // Only capture state for stateful frames
  nsIStatefulFrame* statefulFrame = nsnull;
  aFrame->QueryInterface(NS_GET_IID(nsIStatefulFrame), (void**) &statefulFrame);
  if (!statefulFrame) {
    return NS_OK;
  }

  // Generate the hash key the state was stored under
  // Exit early if we get empty key
  nsresult rv = NS_OK;
  nsCOMPtr<nsIContent> content;
  rv = aFrame->GetContent(getter_AddRefs(content));
  // If we don't have content, we can't generate a hash
  // key and there's probably no state information for us.
  if (!content) {
    return rv;
  }

  nsCAutoString stateKey;
  rv = GenerateStateKey(content, aID, stateKey);
  if(NS_FAILED(rv) || stateKey.IsEmpty()) {
    return rv;
  }

  // Get the state from the hash
  nsCOMPtr<nsIPresState> frameState;
  rv = aState->GetState(stateKey, getter_AddRefs(frameState));
  if (!frameState) {
    return NS_OK;
  }

  // Restore it
  rv = statefulFrame->RestoreState(aPresContext, frameState);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we restore ok, remove the state from the state table
  return aState->RemoveState(stateKey);
}

NS_IMETHODIMP
FrameManager::RestoreFrameState(nsIPresContext* aPresContext, nsIFrame* aFrame, nsILayoutHistoryState* aState)
{
  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(nsnull != aFrame && nsnull != aState, "null parameters passed in");
  
  rv = RestoreFrameStateFor(aPresContext, aFrame, aState);

  // Now restore state recursively for the frame hierarchy rooted at aFrame
  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;
  do {    
    nsIFrame* childFrame;
    aFrame->FirstChild(aPresContext, childListName, &childFrame);
    while (childFrame) {
      rv = RestoreFrameState(aPresContext, childFrame, aState);
      // Get the next sibling child frame
      childFrame->GetNextSibling(&childFrame);
    }
    NS_IF_RELEASE(childListName);
    aFrame->GetAdditionalChildListName(childListIndex++, &childListName);
  } while (childListName);

  return rv;
}


static inline void KeyAppendSep(nsCString& aKey)
{
  if (!aKey.IsEmpty()) {
    aKey.Append(">");
  }
}

static inline void KeyAppendString(nsAReadableString& aString, nsCString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  aKey.Append(NS_ConvertUCS2toUTF8(aString));
}

static inline void KeyAppendInt(PRInt32 aInt, nsCString& aKey)
{
  KeyAppendSep(aKey);

  aKey.AppendInt(aInt);
}

static inline void KeyAppendAtom(nsIAtom* aAtom, nsCString& aKey)
{
  NS_PRECONDITION(aAtom, "KeyAppendAtom: aAtom can not be null!\n");

  const PRUnichar* atomString = nsnull;
  aAtom->GetUnicode(&atomString);

  KeyAppendString(nsDependentString(atomString), aKey);
}

static inline PRBool IsAutocompleteOff(nsIDOMElement* aElement)
{
  nsAutoString autocomplete;
  aElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
  ToLowerCase(autocomplete);
  return autocomplete.Equals(NS_LITERAL_STRING("off"));
}

NS_IMETHODIMP
FrameManager::GenerateStateKey(nsIContent* aContent,
                               nsIStatefulFrame::SpecialStateID aID,
                               nsCString& aKey)
{
  aKey.Truncate();

  // SpecialStateID case - e.g. scrollbars around the content window
  // The key in this case is the special state id (always < min(contentID))
  if (nsIStatefulFrame::eNoID != aID) {
    KeyAppendInt(aID, aKey);
    return NS_OK;
  }

  // We must have content if we're not using a special state id
  NS_ENSURE_TRUE(aContent, NS_ERROR_FAILURE);

  // Don't capture state for anonymous content
  PRUint32 contentID;
  aContent->GetContentID(&contentID);
  if (!contentID) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
  if (element && IsAutocompleteOff(element)) {
    return NS_OK;
  }

  // If we have a dom element, add tag/type/name to hash key
  // This is paranoia, but guarantees that we won't restore
  // state to the wrong type of control.
  nsCOMPtr<nsIAtom> tag;
  if (aContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {

    aContent->GetTag(*getter_AddRefs(tag));
    KeyAppendAtom(tag, aKey);

    nsAutoString name;
    aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name, name);
    KeyAppendString(name, aKey);
  }

  // If we have a form control and can calculate form information, use
  // that as the key - it is more reliable than contentID.
  // Important to have a unique key, and tag/type/name may not be.
  nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
  PRBool generatedUniqueKey = PR_FALSE;
  if (control && mHTMLFormControls && mHTMLForms) {

    if (tag == nsHTMLAtoms::input) {
      PRInt32 type;
      control->GetType(&type);
      KeyAppendInt(type, aKey);
    }

    // If in a form, add form name / index of form / index in form
    PRInt32 index = -1;
    nsCOMPtr<nsIDOMHTMLFormElement> formElement;
    control->GetForm(getter_AddRefs(formElement));
    if (formElement) {

      if (IsAutocompleteOff(formElement)) {
        aKey.Truncate();
        return NS_OK;
      }

      nsAutoString formName;
      formElement->GetName(formName);
      KeyAppendString(formName, aKey);

      nsCOMPtr<nsIContent> formContent(do_QueryInterface(formElement));
      mHTMLForms->IndexOf(formContent, index, PR_FALSE);
      NS_ASSERTION(index > -1,
                   "nsFrameManager::GenerateStateKey didn't find form index!");
      if (index > -1) {
        KeyAppendInt(index, aKey);

        nsCOMPtr<nsIForm> form(do_QueryInterface(formElement));
        form->IndexOfControl(control, &index);
        NS_ASSERTION(index > -1,
                     "nsFrameManager::GenerateStateKey didn't find form control index!");

        if (index > -1) {
          KeyAppendInt(index, aKey);
          generatedUniqueKey = PR_TRUE;
        }
      }

    } else {

      // If not in a form, add index of control in document
      // Less desirable than indexing by form info. 

      // Hash by index of control in doc (we are not in a form)
      // These are important as they are unique, and type/name may not be.
      mHTMLFormControls->IndexOf(aContent, index, PR_FALSE);
      NS_ASSERTION(index > -1,
                   "nsFrameManager::GenerateStateKey didn't find content by type!");
      if (index > -1) {
        KeyAppendInt(index, aKey);
        generatedUniqueKey = PR_TRUE;
      }
    }
  }

  if (!generatedUniqueKey) {

    // Either we didn't have a form control or we aren't in an HTML document
    // so we can't figure out form info, hash by content ID instead :(
    KeyAppendInt(contentID, aKey);
  }

  return NS_OK;
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

MOZ_DECL_CTOR_COUNTER(FrameHashTable)

FrameHashTable::FrameHashTable(PRUint32 aNumBuckets)
{
  MOZ_COUNT_CTOR(FrameHashTable);
  mTable = PL_NewHashTable(aNumBuckets, (PLHashFunction)HashKey,
                           (PLHashComparator)CompareKeys,
                           (PLHashComparator)nsnull,
                           nsnull, nsnull);
}

FrameHashTable::~FrameHashTable()
{
  MOZ_COUNT_DTOR(FrameHashTable);
  PL_HashTableDestroy(mTable);
}

void*
FrameHashTable::Get(void* aKey)
{
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (he) {
    return he->value;
  }
  return nsnull;
}

void*
FrameHashTable::Put(void* aKey, void* aData)
{
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (he) {
    void* oldValue = he->value;
    he->value = aData;
    return oldValue;
  }
  PL_HashTableRawAdd(mTable, hep, hashCode, aKey, aData);
  return nsnull;
}

void*
FrameHashTable::Remove(void* aKey)
{
  PRInt32 hashCode = NS_PTR_TO_INT32(aKey);
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  void* oldValue = nsnull;
  if (he) {
    oldValue = he->value;
    PL_HashTableRawRemove(mTable, hep, he);
  }
  return oldValue;
}

static PRIntn PR_CALLBACK
RemoveEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  // Remove and free this entry and continue enumerating
  return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
}

void
FrameHashTable::Clear()
{
  PL_HashTableEnumerateEntries(mTable, RemoveEntry, 0);
}

#ifdef NS_DEBUG
static PRIntn PR_CALLBACK
EnumEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  // Continue enumerating
  return HT_ENUMERATE_NEXT;
}

void
FrameHashTable::Dump(FILE* fp)
{
  PL_HashTableDump(mTable, EnumEntries, fp);
}
#endif

void
FrameManager::DestroyPropertyList(nsIPresContext* aPresContext)
{
  if (mPropertyList) {
    while (mPropertyList) {
      PropertyList* tmp = mPropertyList;

      mPropertyList = mPropertyList->mNext;
      tmp->RemoveAllProperties(aPresContext);
      delete tmp;
    }
  }
}

FrameManager::PropertyList*
FrameManager::GetPropertyListFor(nsIAtom* aPropertyName) const
{
  PropertyList* result;

  for (result = mPropertyList; result; result = result->mNext) {
    if (result->mName.get() == aPropertyName) {
      break;
    }
  }

  return result;
}

void
FrameManager::RemoveAllPropertiesFor(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  for (PropertyList* prop = mPropertyList; prop; prop = prop->mNext) {
    prop->RemovePropertyForFrame(aPresContext, aFrame);
  }
}

NS_IMETHODIMP
FrameManager::GetFrameProperty(nsIFrame* aFrame,
                               nsIAtom*  aPropertyName,
                               PRUint32  aOptions,
                               void**    aPropertyValue)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aPropertyName);
  PropertyList* propertyList = GetPropertyListFor(aPropertyName);
  nsresult      result;

  if (propertyList) {
    result = propertyList->mFrameValueMap->Search(aFrame, aOptions, aPropertyValue);

  } else {
    *aPropertyValue = 0;
    result = NS_IFRAME_MGR_PROP_NOT_THERE;
  }

  return result;
}

NS_IMETHODIMP
FrameManager::SetFrameProperty(nsIFrame*            aFrame,
                               nsIAtom*             aPropertyName,
                               void*                aPropertyValue,
                               NSFMPropertyDtorFunc aPropDtorFunc)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aPropertyName);
  PropertyList* propertyList = GetPropertyListFor(aPropertyName);
  nsresult      result = NS_OK;

  if (propertyList) {
    // Make sure the dtor function matches
    if (aPropDtorFunc != propertyList->mDtorFunc) {
      return NS_ERROR_INVALID_ARG;
    }

  } else {
    propertyList = new PropertyList(aPropertyName, aPropDtorFunc, mDSTNodeArena);
    if (!propertyList) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    propertyList->mNext = mPropertyList;
    mPropertyList = propertyList;
  }

  // The current property value (if there is one) is replaced and the current
  // value is destroyed
  void* oldValue;
  result = propertyList->mFrameValueMap->Insert(aFrame, aPropertyValue, &oldValue);
  if (NS_DST_VALUE_OVERWRITTEN == result) {
    if (propertyList->mDtorFunc) {
      nsCOMPtr<nsIPresContext> presContext;
      mPresShell->GetPresContext(getter_AddRefs(presContext));

      propertyList->mDtorFunc(presContext, aFrame, aPropertyName, oldValue);
    }
  }

  return result;
}

NS_IMETHODIMP
FrameManager::RemoveFrameProperty(nsIFrame* aFrame,
                                  nsIAtom*  aPropertyName)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG_POINTER(aPropertyName);
  PropertyList* propertyList = GetPropertyListFor(aPropertyName);
  nsresult      result = NS_IFRAME_MGR_PROP_NOT_THERE;

  if (propertyList) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    
    if (propertyList->RemovePropertyForFrame(presContext, aFrame)) {
      result = NS_OK;
    }
  }

  return result;
}

//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(UndisplayedMap)

UndisplayedMap::UndisplayedMap(PRUint32 aNumBuckets)
{
  MOZ_COUNT_CTOR(UndisplayedMap);
  mTable = PL_NewHashTable(aNumBuckets, (PLHashFunction)HashKey,
                           (PLHashComparator)CompareKeys,
                           (PLHashComparator)nsnull,
                           nsnull, nsnull);
  mLastLookup = nsnull;
}

UndisplayedMap::~UndisplayedMap(void)
{
  MOZ_COUNT_DTOR(UndisplayedMap);
  Clear();
  PL_HashTableDestroy(mTable);
}

PLHashEntry**  
UndisplayedMap::GetEntryFor(nsIContent* aParentContent)
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
UndisplayedMap::GetFirstNode(nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  if (*entry) {
    return (UndisplayedNode*)((*entry)->value);
  }
  return nsnull;
}

nsresult      
UndisplayedMap::AppendNodeFor(UndisplayedNode* aNode, nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  if (*entry) {
    UndisplayedNode*  node = (UndisplayedNode*)((*entry)->value);
    while (node->mNext) {
      NS_ASSERTION((node->mContent != aNode->mContent) ||
                   ((node->mContent == nsnull) && 
                    (node->mStyle != aNode->mStyle)), "node in map twice");
      node = node->mNext;
    }
    node->mNext = aNode;
  }
  else {
    PLHashNumber hashCode = NS_PTR_TO_INT32(aParentContent);
    PL_HashTableRawAdd(mTable, entry, hashCode, aParentContent, aNode);
    mLastLookup = nsnull; // hashtable may have shifted bucket out from under us
  }
  return NS_OK;
}

nsresult 
UndisplayedMap::AddNodeFor(nsIContent* aParentContent, nsIContent* aChild, 
                           nsIStyleContext* aStyle)
{
  UndisplayedNode*  node = new UndisplayedNode(aChild, aStyle);
  if (! node) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return AppendNodeFor(node, aParentContent);
}

nsresult
UndisplayedMap::AddNodeFor(nsIContent* aParentContent, nsIStyleContext* aPseudoStyle)
{
  UndisplayedNode*  node = new UndisplayedNode(aPseudoStyle);
  if (! node) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return AppendNodeFor(node, aParentContent);
}

nsresult 
UndisplayedMap::GetNodeFor(nsIContent* aContent, nsIStyleContext** aResult)
{
  PLHashEntry** entry = GetEntryFor(aContent);
  if (*entry) {
    UndisplayedNode* node = (UndisplayedNode*)((*entry)->value);
    *aResult = node->mStyle;
    NS_IF_ADDREF(*aResult);
  }
  else
    *aResult = nsnull;
  return NS_OK;
}

nsresult 
UndisplayedMap::RemoveNodeFor(nsIContent* aParentContent, UndisplayedNode* aNode)
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
  return NS_OK;
}

nsresult
UndisplayedMap::RemoveNodesFor(nsIContent* aParentContent)
{
  PLHashEntry** entry = GetEntryFor(aParentContent);
  NS_ASSERTION(entry, "content not in map");
  if (*entry) {
    UndisplayedNode*  node = (UndisplayedNode*)((*entry)->value);
    delete node;
    if (entry == mLastLookup) {
      mLastLookup = nsnull;
    }
    PL_HashTableRawRemove(mTable, entry, *entry);
  }
  return NS_OK;
}

static PRIntn PR_CALLBACK
RemoveUndisplayedEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  UndisplayedNode*  node = (UndisplayedNode*)(he->value);
  delete node;
  // Remove and free this entry and continue enumerating
  return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
}

void
UndisplayedMap::Clear(void)
{
  mLastLookup = nsnull;
  PL_HashTableEnumerateEntries(mTable, RemoveUndisplayedEntry, 0);
}

//----------------------------------------------------------------------
    
FrameManager::PropertyList::PropertyList(nsIAtom*             aName,
                                         NSFMPropertyDtorFunc aDtorFunc,
                                         nsDST::NodeArena*    aDSTNodeArena)
  : mName(aName), mFrameValueMap(new nsDST(aDSTNodeArena)),
    mDtorFunc(aDtorFunc), mNext(nsnull)
{
}

FrameManager::PropertyList::~PropertyList()
{
  delete mFrameValueMap;
}

class DestroyPropertyValuesFunctor : public nsDSTNodeFunctor {
public:
  DestroyPropertyValuesFunctor(nsIPresContext*      aPresContext,
                               nsIAtom*             aPropertyName,
                               NSFMPropertyDtorFunc aDtorFunc)
    : mPresContext(aPresContext), mPropertyName(aPropertyName), mDtorFunc(aDtorFunc) {}

  virtual void operator () (void *aKey, void *aValue) {
    mDtorFunc(mPresContext, (nsIFrame*)aKey, mPropertyName, aValue);
  }

  nsIPresContext*      mPresContext;
  nsIAtom*             mPropertyName;
  NSFMPropertyDtorFunc mDtorFunc;
};

void
FrameManager::PropertyList::RemoveAllProperties(nsIPresContext* aPresContext)
{
  if (mDtorFunc) {
    DestroyPropertyValuesFunctor  functor(aPresContext, mName, mDtorFunc);

    // Enumerate any remaining frame/value pairs and destroy the value object
    mFrameValueMap->Enumerate(functor);
  }
}

PRBool
FrameManager::PropertyList::RemovePropertyForFrame(nsIPresContext* aPresContext,
                                                   nsIFrame*       aFrame)
{
  void*     value;
  nsresult  result;
   
  // If the property exists, then we need to run the dtor function so
  // do a search with the option to remove the key/value pair
  result = mFrameValueMap->Search(aFrame, NS_DST_REMOVE_KEY_VALUE, &value);

  if (NS_OK == result) {
    // The property was set
    if (mDtorFunc) {
      // Destroy the property value
      mDtorFunc(aPresContext, aFrame, mName, value);
    }

    return PR_TRUE;
  }

  return PR_FALSE;
}

