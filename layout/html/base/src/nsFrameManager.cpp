/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIFrameManager.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "plhash.h"
#include "nsDST.h"
#include "nsPlaceholderFrame.h"
#ifdef NS_DEBUG
#include "nsLayoutAtoms.h"
#endif

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// IID's
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);

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

//----------------------------------------------------------------------

class FrameManager;

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

  // Functions for manipulating the frame model
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIFrame*       aParentFrame,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);
  NS_IMETHOD ReplaceFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame,
                          nsIFrame*       aNewFrame);
  
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);

private:
  nsIPresShell*                   mPresShell;  // weak link, because the pres shell owns us
  nsIStyleSet*                    mStyleSet;   // weak link. pres shell holds a reference
  nsDST*                          mPrimaryFrameMap;
  FrameHashTable*                 mPlaceholderMap;
  CantRenderReplacedElementEvent* mPostedEvents;

  void HandleCantRenderReplacedElementEvent(CantRenderReplacedElementEvent* aEvent);
  void RevokePostedEvents();
  CantRenderReplacedElementEvent** FindPostedEventFor(nsIFrame* aFrame);
  void DequeuePostedEventFor(nsIFrame* aFrame);

  friend void PR_CALLBACK HandlePLEvent(CantRenderReplacedElementEvent* aEvent);
};

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewFrameManager(nsIFrameManager** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  FrameManager* it = new FrameManager;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(nsIFrameManager::GetIID(), (void **)aInstancePtrResult);
}

FrameManager::FrameManager()
{
}

NS_IMPL_ADDREF(FrameManager)
NS_IMPL_RELEASE(FrameManager)

FrameManager::~FrameManager()
{
  // Revoke any events posted to the event queue that we haven't processed yet
  RevokePostedEvents();
  
  delete mPrimaryFrameMap;
  delete mPlaceholderMap;
}

nsresult
FrameManager::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (aIID.Equals(GetIID())) {
    *aInstancePtr = (void*)(nsIFrameManager*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
FrameManager::Init(nsIPresShell* aPresShell,
                   nsIStyleSet*  aStyleSet)
{
  mPresShell = aPresShell;
  mStyleSet = aStyleSet;
  return NS_OK;
}

//----------------------------------------------------------------------

// Primary frame functions
NS_IMETHODIMP
FrameManager::GetPrimaryFrameFor(nsIContent* aContent, nsIFrame** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  NS_PRECONDITION(nsnull != aContent, "no content object");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull == mPrimaryFrameMap) {
    *aResult = nsnull;
  } else {
    *aResult = (nsIFrame*)mPrimaryFrameMap->Search(aContent);
    if (!*aResult) {
      nsCOMPtr<nsIStyleSet>    styleSet;
      nsCOMPtr<nsIPresContext> presContext;

      // Give the frame construction code the opportunity to return the
      // frame that maps the content object
      mPresShell->GetStyleSet(getter_AddRefs(styleSet));
      mPresShell->GetPresContext(getter_AddRefs(presContext));
      styleSet->FindPrimaryFrameFor(presContext, this, aContent, aResult);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::SetPrimaryFrameFor(nsIContent* aContent,
                                 nsIFrame*   aPrimaryFrame)
{
  NS_PRECONDITION(aContent, "no content object");

  // If aPrimaryFrame is NULL, then remove the mapping
  if (!aPrimaryFrame) {
    if (mPrimaryFrameMap) {
      mPrimaryFrameMap->Remove(aContent);
    }
  } else {
    // Create a new DST if necessary
    if (!mPrimaryFrameMap) {
      mPrimaryFrameMap = new nsDST;
      if (!mPrimaryFrameMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    // Add a mapping to the hash table
    nsIFrame* oldPrimaryFrame;
     
    oldPrimaryFrame = (nsIFrame*)mPrimaryFrameMap->Insert(aContent, (void*)aPrimaryFrame);
#ifdef NS_DEBUG
    if (oldPrimaryFrame && (oldPrimaryFrame != aPrimaryFrame)) {
      NS_WARNING("overwriting current primary frame");
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
FrameManager::ClearPrimaryFrameMap()
{
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
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  NS_PRECONDITION(nsnull != aFrame, "no frame");
  if ((nsnull == aResult) || (nsnull == aFrame)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull == mPlaceholderMap) {
    *aResult = nsnull;
  } else {
    *aResult = (nsIFrame*)mPlaceholderMap->Get(aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
FrameManager::SetPlaceholderFrameFor(nsIFrame* aFrame,
                                     nsIFrame* aPlaceholderFrame)
{
  NS_PRECONDITION(aFrame, "no frame");
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
  if (mPlaceholderMap) {
    mPlaceholderMap->Clear();
  }
  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
FrameManager::AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  return aParentFrame->AppendFrames(aPresContext, aPresShell, aListName,
                                    aFrameList);
}

NS_IMETHODIMP
FrameManager::InsertFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList)
{
  return aParentFrame->InsertFrames(aPresContext, aPresShell, aListName,
                                    aPrevFrame, aFrameList);
}

NS_IMETHODIMP
FrameManager::RemoveFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame)
{
  return aParentFrame->RemoveFrame(aPresContext, aPresShell, aListName,
                                   aOldFrame);
}

NS_IMETHODIMP
FrameManager::ReplaceFrame(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIFrame*       aParentFrame,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame)
{
  return aParentFrame->ReplaceFrame(aPresContext, aPresShell, aListName,
                                    aOldFrame, aNewFrame);
}

//----------------------------------------------------------------------

NS_IMETHODIMP
FrameManager::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // Dequeue and destroy and posted events for this frame
  DequeuePostedEventFor(aFrame);
  return NS_OK;
}

void
FrameManager::RevokePostedEvents()
{
  if (mPostedEvents) {
    mPostedEvents = nsnull;

    // Revoke any events in the event queue that are owned by us
    nsIEventQueueService* eventService;
    nsresult              rv;

    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      kIEventQueueServiceIID,
                                      (nsISupports **)&eventService);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                             getter_AddRefs(eventQueue));
      nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

      if (NS_SUCCEEDED(rv) && eventQueue) {
        eventQueue->RevokeEvents(this);
      }
    }
  }
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
                                      kIEventQueueServiceIID,
                                      (nsISupports **)&eventService);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEventQueue> eventQueue;
      rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                             getter_AddRefs(eventQueue));
      nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

      if (NS_SUCCEEDED(rv) && eventQueue) {
        PLEventQueue* plqueue;

        eventQueue->GetPLEventQueue(&plqueue);
        if (plqueue) {
          // Removes the event and destroys it
          PL_DequeueEvent(tmp, plqueue);
        }
      }
    }
  }
}

void
FrameManager::HandleCantRenderReplacedElementEvent(CantRenderReplacedElementEvent* aEvent)
{
  // Remove the posted event from our linked list
  CantRenderReplacedElementEvent** events = &mPostedEvents;
  while (*events) {
    if (*events == aEvent) {
      *events = (*events)->mNext;
      break;
    } else {
      events = &(*events)->mNext;
    }
  }

  // Notify the style system and then process any reflow commands that
  // are generated
  nsCOMPtr<nsIPresContext>  presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));
  mStyleSet->CantRenderReplacedElement(presContext, aEvent->mFrame);
  mPresShell->ProcessReflowCommands();
}

void PR_CALLBACK
HandlePLEvent(CantRenderReplacedElementEvent* aEvent)
{
  FrameManager* frameManager = (FrameManager*)aEvent->owner;
  frameManager->HandleCantRenderReplacedElementEvent(aEvent);
}

static void PR_CALLBACK
DestroyPLEvent(CantRenderReplacedElementEvent* aEvent)
{
  delete aEvent;
}

CantRenderReplacedElementEvent::CantRenderReplacedElementEvent(FrameManager* aFrameManager,
                                                               nsIFrame*     aFrame)
{
  // Note: because the frame manager owns us we don't hold a reference to the
  // frame manager
  PL_InitEvent(this, aFrameManager, (PLHandleEventProc)::HandlePLEvent,
               (PLDestroyEventProc)::DestroyPLEvent);
  mFrame = aFrame;
}

NS_IMETHODIMP
FrameManager::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                        nsIFrame*       aFrame)
{
  nsIEventQueueService* eventService;
  nsresult              rv;

  // We need to notify the style stystem, but post the notification so it
  // doesn't happen now
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&eventService);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEventQueue> eventQueue;
    rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                           getter_AddRefs(eventQueue));
    nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

    if (NS_SUCCEEDED(rv) && eventQueue) {
#ifdef NS_DEBUG
      // Verify that there isn't already a posted event associated with
      // this frame
      NS_ASSERTION(!*FindPostedEventFor(aFrame), "frame already has posted event");
#endif
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

//----------------------------------------------------------------------

static PLHashNumber
HashKey(void* key)
{
  return (PLHashNumber)key;
}

static PRIntn
CompareKeys(void* key1, void* key2)
{
  return key1 == key2;
}

FrameHashTable::FrameHashTable(PRUint32 aNumBuckets)
{
  mTable = PL_NewHashTable(aNumBuckets, (PLHashFunction)HashKey,
                           (PLHashComparator)CompareKeys,
                           (PLHashComparator)nsnull,
                           nsnull, nsnull);
}

FrameHashTable::~FrameHashTable()
{
  PL_HashTableDestroy(mTable);
}

void*
FrameHashTable::Get(void* aKey)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    return he->value;
  }
  return nsnull;
}

void*
FrameHashTable::Put(void* aKey, void* aData)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
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
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  void* oldValue = nsnull;
  if (nsnull != he) {
    oldValue = he->value;
    PL_HashTableRawRemove(mTable, hep, he);
  }
  return oldValue;
}

static PRIntn
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
static PRIntn
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

