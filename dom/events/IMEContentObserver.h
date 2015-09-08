/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class nsIContent;
class nsINode;
class nsISelection;
class nsPresContext;

namespace mozilla {

class EventStateManager;

// IMEContentObserver notifies widget of any text and selection changes
// in the currently focused editor
class IMEContentObserver final : public nsISelectionListener
                               , public nsStubMutationObserver
                               , public nsIReflowObserver
                               , public nsIScrollObserver
                               , public nsSupportsWeakReference
                               , public nsIEditorObserver
{
public:
  typedef widget::IMENotification::SelectionChangeData SelectionChangeData;
  typedef widget::IMENotification::TextChangeData TextChangeData;
  typedef widget::IMENotification::TextChangeDataBase TextChangeDataBase;

  IMEContentObserver();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IMEContentObserver,
                                           nsISelectionListener)
  NS_DECL_NSIEDITOROBSERVER
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() override;

  bool OnMouseButtonEvent(nsPresContext* aPresContext,
                          WidgetMouseEvent* aMouseEvent);

  nsresult HandleQueryContentEvent(WidgetQueryContentEvent* aEvent);

  void Init(nsIWidget* aWidget, nsPresContext* aPresContext,
            nsIContent* aContent, nsIEditor* aEditor);
  void Destroy();
  /**
   * IMEContentObserver is stored by EventStateManager during observing.
   * DisconnectFromEventStateManager() is called when EventStateManager stops
   * storing the instance.
   */
  void DisconnectFromEventStateManager();
  /**
   * MaybeReinitialize() tries to restart to observe the editor's root node.
   * This is useful when the editor is reframed and all children are replaced
   * with new node instances.
   * @return            Returns true if the instance is managing the content.
   *                    Otherwise, false.
   */
  bool MaybeReinitialize(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIEditor* aEditor);
  bool IsManaging(nsPresContext* aPresContext, nsIContent* aContent);
  bool IsEditorHandlingEventForComposition() const;
  bool KeepAliveDuringDeactive() const
  {
    return mUpdatePreference.WantDuringDeactive();
  }
  nsIWidget* GetWidget() const { return mWidget; }
  nsIEditor* GetEditor() const { return mEditor; }
  void SuppressNotifyingIME();
  void UnsuppressNotifyingIME();
  nsPresContext* GetPresContext() const;
  nsresult GetSelectionAndRoot(nsISelection** aSelection,
                               nsIContent** aRoot) const;

private:
  ~IMEContentObserver() {}

  enum State {
    eState_NotObserving,
    eState_Initializing,
    eState_StoppedObserving,
    eState_Observing
  };
  State GetState() const;
  bool IsObservingContent(nsPresContext* aPresContext,
                          nsIContent* aContent) const;
  bool IsReflowLocked() const;
  bool IsSafeToNotifyIME() const;

  void PostFocusSetNotification();
  void MaybeNotifyIMEOfFocusSet();
  void PostTextChangeNotification(const TextChangeDataBase& aTextChangeData);
  void MaybeNotifyIMEOfTextChange(const TextChangeDataBase& aTextChangeData);
  void PostSelectionChangeNotification(bool aCausedByComposition,
                                       bool aCausedBySelectionEvent);
  void MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition,
                                       bool aCausedBySelectionEvent);
  void PostPositionChangeNotification();
  void MaybeNotifyIMEOfPositionChange();

  void NotifyContentAdded(nsINode* aContainer, int32_t aStart, int32_t aEnd);
  void ObserveEditableNode();
  /**
   *  NotifyIMEOfBlur() notifies IME of blur.
   */
  void NotifyIMEOfBlur();
  /**
   *  UnregisterObservers() unregisters all listeners and observers.
   */
  void UnregisterObservers();
  void FlushMergeableNotifications();
  void ClearPendingNotifications()
  {
    mIsFocusEventPending = false;
    mIsSelectionChangeEventPending = false;
    mIsPositionChangeEventPending = false;
    mTextChangeData.Clear();
  }

  /**
   * UpdateSelectionCache() updates mSelectionData with the latest selection.
   * This should be called only when IsSafeToNotifyIME() returns true.
   *
   * Note that this does nothing if mUpdatePreference.WantSelectionChange()
   * returns false.
   */
  bool UpdateSelectionCache();

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
  // mStartOfRemovingTextRangeCache caches text length from the start of content
  // to the start of the last removed content only while an edit action is being
  // handled by the editor and no other mutation (e.g., adding node) occur.
  FlatTextCache mStartOfRemovingTextRangeCache;

  TextChangeData mTextChangeData;

  // mSelectionData is the last selection data which was notified.  This is
  // modified by UpdateSelectionCache().  Note that mCausedBy* are always
  // false.  Do NOT refer them.
  SelectionChangeData mSelectionData;

  EventStateManager* mESM;

  nsIMEUpdatePreference mUpdatePreference;
  uint32_t mPreAttrChangeLength;
  uint32_t mSuppressNotifications;
  int64_t mPreCharacterDataChangeLength;

  bool mIsObserving;
  bool mIMEHasFocus;
  bool mIsFocusEventPending;
  bool mIsSelectionChangeEventPending;
  bool mSelectionChangeCausedOnlyByComposition;
  bool mSelectionChangeCausedOnlyBySelectionEvent;
  bool mIsPositionChangeEventPending;
  bool mIsFlushingPendingNotifications;


  /**
   * Helper classes to notify IME.
   */

  class AChangeEvent: public nsRunnable
  {
  protected:
    enum ChangeEventType
    {
      eChangeEventType_Focus,
      eChangeEventType_Selection,
      eChangeEventType_Text,
      eChangeEventType_Position,
      eChangeEventType_FlushPendingEvents
    };

    AChangeEvent(ChangeEventType aChangeEventType,
                 IMEContentObserver* aIMEContentObserver)
      : mIMEContentObserver(aIMEContentObserver)
      , mChangeEventType(aChangeEventType)
    {
      MOZ_ASSERT(mIMEContentObserver);
    }

    nsRefPtr<IMEContentObserver> mIMEContentObserver;
    ChangeEventType mChangeEventType;

    /**
     * CanNotifyIME() checks if mIMEContentObserver can and should notify IME.
     */
    bool CanNotifyIME() const;

    /**
     * IsSafeToNotifyIME() checks if it's safe to noitify IME.
     */
    bool IsSafeToNotifyIME() const;
  };

  class FocusSetEvent: public AChangeEvent
  {
  public:
    explicit FocusSetEvent(IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_Focus, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };

  class SelectionChangeEvent : public AChangeEvent
  {
  public:
    SelectionChangeEvent(IMEContentObserver* aIMEContentObserver,
                         bool aCausedByComposition,
                         bool aCausedBySelectionEvent)
      : AChangeEvent(eChangeEventType_Selection, aIMEContentObserver)
      , mCausedByComposition(aCausedByComposition)
      , mCausedBySelectionEvent(aCausedBySelectionEvent)
    {
      aIMEContentObserver->mSelectionChangeCausedOnlyByComposition = false;
      aIMEContentObserver->mSelectionChangeCausedOnlyBySelectionEvent = false;
    }
    NS_IMETHOD Run() override;

  private:
    bool mCausedByComposition;
    bool mCausedBySelectionEvent;
  };

  class TextChangeEvent : public AChangeEvent
  {
  public:
    TextChangeEvent(IMEContentObserver* aIMEContentObserver,
                    TextChangeDataBase& aTextChangeData)
      : AChangeEvent(eChangeEventType_Text, aIMEContentObserver)
      , mTextChangeData(aTextChangeData)
    {
      MOZ_ASSERT(mTextChangeData.IsValid());
      // Reset aTextChangeData because this now consumes the data.
      aTextChangeData.Clear();
    }
    NS_IMETHOD Run() override;

  private:
    TextChangeDataBase mTextChangeData;
  };

  class PositionChangeEvent final : public AChangeEvent
  {
  public:
    explicit PositionChangeEvent(IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_Position, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };

  class AsyncMergeableNotificationsFlusher : public AChangeEvent
  {
  public:
    explicit AsyncMergeableNotificationsFlusher(
      IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_FlushPendingEvents, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };
};

} // namespace mozilla

#endif // mozilla_IMEContentObserver_h_
