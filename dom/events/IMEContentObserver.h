/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IMEContentObserver_h_
#define mozilla_IMEContentObserver_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h" // XXX Why does only this need to be included here?
#include "nsIEditor.h"
#include "nsIEditorObserver.h"
#include "nsIReflowObserver.h"
#include "nsISelectionListener.h"
#include "nsIScrollObserver.h"
#include "nsIWidget.h" // for nsIMEUpdatePreference
#include "nsStubMutationObserver.h"
#include "nsWeakReference.h"

class nsIContent;
class nsINode;
class nsISelection;
class nsPresContext;

namespace mozilla {

class EventStateManager;

// IMEContentObserver notifies widget of any text and selection changes
// in the currently focused editor
class IMEContentObserver MOZ_FINAL : public nsISelectionListener
                                   , public nsStubMutationObserver
                                   , public nsIReflowObserver
                                   , public nsIScrollObserver
                                   , public nsSupportsWeakReference
                                   , public nsIEditorObserver
{
public:
  IMEContentObserver();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IMEContentObserver,
                                           nsISelectionListener)
  NS_DECL_NSIEDITOROBSERVER
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() MOZ_OVERRIDE;

  void Init(nsIWidget* aWidget, nsPresContext* aPresContext,
            nsIContent* aContent);
  void Destroy();
  /**
   * IMEContentObserver is stored by EventStateManager during observing.
   * DisconnectFromEventStateManager() is called when EventStateManager stops
   * storing the instance.
   */
  void DisconnectFromEventStateManager();
  bool IsManaging(nsPresContext* aPresContext, nsIContent* aContent);
  bool IsEditorHandlingEventForComposition() const;
  bool KeepAliveDuringDeactive() const
  {
    return mUpdatePreference.WantDuringDeactive();
  }
  nsIWidget* GetWidget() const { return mWidget; }
  nsresult GetSelectionAndRoot(nsISelection** aSelection,
                               nsIContent** aRoot) const;

  struct TextChangeData
  {
    // mStartOffset is the start offset of modified or removed text in
    // original content and inserted text in new content.
    uint32_t mStartOffset;
    // mRemovalEndOffset is the end offset of modified or removed text in
    // original content.  If the value is same as mStartOffset, no text hasn't
    // been removed yet.
    uint32_t mRemovedEndOffset;
    // mAddedEndOffset is the end offset of inserted text or same as
    // mStartOffset if just removed.  The vlaue is offset in the new content.
    uint32_t mAddedEndOffset;

    bool mCausedOnlyByComposition;
    bool mStored;

    TextChangeData()
      : mStartOffset(0)
      , mRemovedEndOffset(0)
      , mAddedEndOffset(0)
      , mCausedOnlyByComposition(false)
      , mStored(false)
    {
    }

    TextChangeData(uint32_t aStartOffset,
                   uint32_t aRemovedEndOffset,
                   uint32_t aAddedEndOffset,
                   bool aCausedByComposition)
      : mStartOffset(aStartOffset)
      , mRemovedEndOffset(aRemovedEndOffset)
      , mAddedEndOffset(aAddedEndOffset)
      , mCausedOnlyByComposition(aCausedByComposition)
      , mStored(true)
    {
      MOZ_ASSERT(aRemovedEndOffset >= aStartOffset,
                 "removed end offset must not be smaller than start offset");
      MOZ_ASSERT(aAddedEndOffset >= aStartOffset,
                 "added end offset must not be smaller than start offset");
    }
    // Positive if text is added. Negative if text is removed.
    int64_t Difference() const 
    {
      return mAddedEndOffset - mRemovedEndOffset;
    }
  };

private:
  ~IMEContentObserver() {}

  void MaybeNotifyIMEOfTextChange(const TextChangeData& aTextChangeData);
  void MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition);
  void MaybeNotifyIMEOfPositionChange();

  void NotifyContentAdded(nsINode* aContainer, int32_t aStart, int32_t aEnd);
  void ObserveEditableNode();
  /**
   *  UnregisterObservers() unresiters all listeners and observers.
   *  @param aPostEvent     When true, DOM event will be posted to the thread.
   *                        Otherwise, dispatched when safe.
   */
  void UnregisterObservers(bool aPostEvent);
  void StoreTextChangeData(const TextChangeData& aTextChangeData);
  void FlushMergeableNotifications();

#ifdef DEBUG
  void TestMergingTextChangeData();
#endif

  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsISelection> mSelection;
  nsCOMPtr<nsIContent> mRootContent;
  nsCOMPtr<nsINode> mEditableNode;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIEditor> mEditor;

  /**
   * FlatTextCache stores flat text length from start of the content to
   * mNodeOffset of mContainerNode.
   */
  struct FlatTextCache
  {
    // mContainerNode and mNodeOffset represent a point in DOM tree.  E.g.,
    // if mContainerNode is a div element, mNodeOffset is index of its child.
    nsCOMPtr<nsINode> mContainerNode;
    int32_t mNodeOffset;
    // Length of flat text generated from contents between the start of content
    // and a child node whose index is mNodeOffset of mContainerNode.
    uint32_t mFlatTextLength;

    FlatTextCache()
      : mNodeOffset(0)
      , mFlatTextLength(0)
    {
    }

    void Clear()
    {
      mContainerNode = nullptr;
      mNodeOffset = 0;
      mFlatTextLength = 0;
    }

    void Cache(nsINode* aContainer, int32_t aNodeOffset,
               uint32_t aFlatTextLength)
    {
      MOZ_ASSERT(aContainer, "aContainer must not be null");
      MOZ_ASSERT(
        aNodeOffset <= static_cast<int32_t>(aContainer->GetChildCount()),
        "aNodeOffset must be same as or less than the count of children");
      mContainerNode = aContainer;
      mNodeOffset = aNodeOffset;
      mFlatTextLength = aFlatTextLength;
    }

    bool Match(nsINode* aContainer, int32_t aNodeOffset) const
    {
      return aContainer == mContainerNode && aNodeOffset == mNodeOffset;
    }
  };
  // mEndOfAddedTextCache caches text length from the start of content to
  // the end of the last added content only while an edit action is being
  // handled by the editor and no other mutation (e.g., removing node)
  // occur.
  FlatTextCache mEndOfAddedTextCache;

  TextChangeData mTextChangeData;

  EventStateManager* mESM;

  nsIMEUpdatePreference mUpdatePreference;
  uint32_t mPreAttrChangeLength;

  bool mIsEditorInTransaction;
  bool mIsSelectionChangeEventPending;
  bool mSelectionChangeCausedOnlyByComposition;
  bool mIsPositionChangeEventPending;
};

} // namespace mozilla

#endif // mozilla_IMEContentObserver_h_
