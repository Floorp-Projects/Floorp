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
#include "nsIWidget.h"
#include "nsStubDocumentObserver.h"
#include "nsStubMutationObserver.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class nsIContent;
class nsINode;
class nsISelection;
class nsPresContext;

namespace mozilla {

class EventStateManager;
class TextComposition;

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
  typedef ContentEventHandler::NodePosition NodePosition;
  typedef ContentEventHandler::NodePositionBefore NodePositionBefore;
  typedef widget::IMENotification::SelectionChangeData SelectionChangeData;
  typedef widget::IMENotification::TextChangeData TextChangeData;
  typedef widget::IMENotification::TextChangeDataBase TextChangeDataBase;
  typedef widget::IMENotificationRequests IMENotificationRequests;
  typedef widget::IMEMessage IMEMessage;

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

  /**
   * Init() initializes the instance, i.e., retrieving necessary objects and
   * starts to observe something.
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   *
   * @param aWidget         The widget which can access native IME.
   * @param aPresContext    The PresContext which has aContent.
   * @param aContent        An editable element or a plugin host element which
   *                        user may use IME in.
   *                        Or nullptr if this will observe design mode
   *                        document.
   * @param aEditor         When aContent is an editable element or nullptr,
   *                        non-nullptr referring an editor instance which
   *                        manages aContent.
   *                        Otherwise, i.e., this will observe a plugin content,
   *                        should be nullptr.
   */
  void Init(nsIWidget* aWidget, nsPresContext* aPresContext,
            nsIContent* aContent, nsIEditor* aEditor);

  /**
   * Destroy() finalizes the instance, i.e., stops observing contents and
   * clearing the members.
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   */
  void Destroy();

  /**
   * Returns false if the instance refers some objects and observing them.
   * Otherwise, true.
   */
  bool Destroyed() const;

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
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   *
   * @return            Returns true if the instance is managing the content.
   *                    Otherwise, false.
   */
  bool MaybeReinitialize(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIEditor* aEditor);

  bool IsManaging(nsPresContext* aPresContext, nsIContent* aContent) const;
  bool IsManaging(const TextComposition* aTextComposition) const;
  bool WasInitializedWithPlugin() const;
  bool IsEditorHandlingEventForComposition() const;
  bool KeepAliveDuringDeactive() const
  {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantDuringDeactive();
  }
  nsIWidget* GetWidget() const { return mWidget; }
  nsIEditor* GetEditor() const { return mEditor; }
  void SuppressNotifyingIME();
  void UnsuppressNotifyingIME();
  nsPresContext* GetPresContext() const;
  nsresult GetSelectionAndRoot(nsISelection** aSelection,
                               nsIContent** aRoot) const;

  /**
   * TryToFlushPendingNotifications() should be called when pending events
   * should be flushed.  This tries to run the queued IMENotificationSender.
   */
  void TryToFlushPendingNotifications();

  /**
   * MaybeNotifyCompositionEventHandled() posts composition event handled
   * notification into the pseudo queue.
   */
  void MaybeNotifyCompositionEventHandled();

private:
  ~IMEContentObserver() {}

  enum State {
    eState_NotObserving,
    eState_Initializing,
    eState_StoppedObserving,
    eState_Observing
  };
  State GetState() const;
  bool InitWithEditor(nsPresContext* aPresContext, nsIContent* aContent,
                      nsIEditor* aEditor);
  bool InitWithPlugin(nsPresContext* aPresContext, nsIContent* aContent);
  bool IsInitializedWithPlugin() const { return !mEditor; }
  void OnIMEReceivedFocus();
  void Clear();
  bool IsObservingContent(nsPresContext* aPresContext,
                          nsIContent* aContent) const;
  bool IsReflowLocked() const;
  bool IsSafeToNotifyIME() const;
  bool IsEditorComposing() const;

  // Following methods are called by DocumentObserver when
  // beginning to update the contents and ending updating the contents.
  void BeginDocumentUpdate();
  void EndDocumentUpdate();

  void PostFocusSetNotification();
  void MaybeNotifyIMEOfFocusSet();
  void PostTextChangeNotification();
  void MaybeNotifyIMEOfTextChange(const TextChangeDataBase& aTextChangeData);
  void CancelNotifyingIMEOfTextChange();
  void PostSelectionChangeNotification();
  void MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition,
                                       bool aCausedBySelectionEvent,
                                       bool aOccurredDuringComposition);
  void PostPositionChangeNotification();
  void MaybeNotifyIMEOfPositionChange();
  void CancelNotifyingIMEOfPositionChange();
  void PostCompositionEventHandledNotification();

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
  bool NeedsTextChangeNotification() const
  {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantTextChange();
  }
  bool NeedsPositionChangeNotification() const
  {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantPositionChanged();
  }
  void ClearPendingNotifications()
  {
    mNeedsToNotifyIMEOfFocusSet = false;
    mNeedsToNotifyIMEOfTextChange = false;
    mNeedsToNotifyIMEOfSelectionChange = false;
    mNeedsToNotifyIMEOfPositionChange = false;
    mNeedsToNotifyIMEOfCompositionEventHandled = false;
    mTextChangeData.Clear();
  }
  bool NeedsToNotifyIMEOfSomething() const
  {
    return mNeedsToNotifyIMEOfFocusSet ||
           mNeedsToNotifyIMEOfTextChange ||
           mNeedsToNotifyIMEOfSelectionChange ||
           mNeedsToNotifyIMEOfPositionChange ||
           mNeedsToNotifyIMEOfCompositionEventHandled;
  }

  /**
   * UpdateSelectionCache() updates mSelectionData with the latest selection.
   * This should be called only when IsSafeToNotifyIME() returns true.
   *
   * Note that this does nothing if WasInitializedWithPlugin() returns true.
   */
  bool UpdateSelectionCache();

  nsCOMPtr<nsIWidget> mWidget;
  // mFocusedWidget has the editor observed by the instance.  E.g., if the
  // focused editor is in XUL panel, this should be the widget of the panel.
  // On the other hand, mWidget is its parent which handles IME.
  nsCOMPtr<nsIWidget> mFocusedWidget;
  nsCOMPtr<nsISelection> mSelection;
  nsCOMPtr<nsIContent> mRootContent;
  nsCOMPtr<nsINode> mEditableNode;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIEditor> mEditor;

  /**
   * Helper classes to notify IME.
   */

  class AChangeEvent: public Runnable
  {
  protected:
    enum ChangeEventType
    {
      eChangeEventType_Focus,
      eChangeEventType_Selection,
      eChangeEventType_Text,
      eChangeEventType_Position,
      eChangeEventType_CompositionEventHandled
    };

    explicit AChangeEvent(const char* aName,
                          IMEContentObserver* aIMEContentObserver)
      : Runnable(aName)
      , mIMEContentObserver(aIMEContentObserver)
    {
      MOZ_ASSERT(mIMEContentObserver);
    }

    RefPtr<IMEContentObserver> mIMEContentObserver;

    /**
     * CanNotifyIME() checks if mIMEContentObserver can and should notify IME.
     */
    bool CanNotifyIME(ChangeEventType aChangeEventType) const;

    /**
     * IsSafeToNotifyIME() checks if it's safe to noitify IME.
     */
    bool IsSafeToNotifyIME(ChangeEventType aChangeEventType) const;
  };

  class IMENotificationSender: public AChangeEvent
  {
  public:
    explicit IMENotificationSender(IMEContentObserver* aIMEContentObserver)
      : AChangeEvent("IMENotificationSender", aIMEContentObserver)
      , mIsRunning(false)
    {
    }
    NS_IMETHOD Run() override;

  private:
    void SendFocusSet();
    void SendSelectionChange();
    void SendTextChange();
    void SendPositionChange();
    void SendCompositionEventHandled();

    bool mIsRunning;
  };

  // mQueuedSender is, it was put into the event queue but not run yet.
  RefPtr<IMENotificationSender> mQueuedSender;

  /**
   * IMEContentObserver is a mutation observer of mRootContent.  However,
   * it needs to know the beginning of content changes and end of it too for
   * reducing redundant computation of text offset with ContentEventHandler.
   * Therefore, it needs helper class to listen only them since if
   * both mutations were observed by IMEContentObserver directly, each
   * methods need to check if the changing node is in mRootContent but it's
   * too expensive.
   */
  class DocumentObserver final : public nsStubDocumentObserver
  {
  public:
    explicit DocumentObserver(IMEContentObserver& aIMEContentObserver)
      : mIMEContentObserver(&aIMEContentObserver)
      , mDocumentUpdating(0)
    {
    }

    NS_DECL_CYCLE_COLLECTION_CLASS(DocumentObserver)
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE
    NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE

    void Observe(nsIDocument* aDocument);
    void StopObserving();
    void Destroy();

    bool Destroyed() const { return !mIMEContentObserver; }
    bool IsObserving() const { return mDocument != nullptr; }
    bool IsUpdating() const { return mDocumentUpdating != 0; }

  private:
    DocumentObserver() = delete;
    virtual ~DocumentObserver() { Destroy(); }

    RefPtr<IMEContentObserver> mIMEContentObserver;
    nsCOMPtr<nsIDocument> mDocument;
    uint32_t mDocumentUpdating;
  };
  RefPtr<DocumentObserver> mDocumentObserver;

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

  // mSelectionData is the last selection data which was notified.  The
  // selection information is modified by UpdateSelectionCache().  The reason
  // of the selection change is modified by MaybeNotifyIMEOfSelectionChange().
  SelectionChangeData mSelectionData;

  EventStateManager* mESM;

  const IMENotificationRequests* mIMENotificationRequests;
  uint32_t mPreAttrChangeLength;
  uint32_t mSuppressNotifications;
  int64_t mPreCharacterDataChangeLength;

  // mSendingNotification is a notification which is now sending from
  // IMENotificationSender.  When the value is NOTIFY_IME_OF_NOTHING, it's
  // not sending any notification.
  IMEMessage mSendingNotification;

  bool mIsObserving;
  bool mIMEHasFocus;
  bool mNeedsToNotifyIMEOfFocusSet;
  bool mNeedsToNotifyIMEOfTextChange;
  bool mNeedsToNotifyIMEOfSelectionChange;
  bool mNeedsToNotifyIMEOfPositionChange;
  bool mNeedsToNotifyIMEOfCompositionEventHandled;
  // mIsHandlingQueryContentEvent is true when IMEContentObserver is handling
  // WidgetQueryContentEvent with ContentEventHandler.
  bool mIsHandlingQueryContentEvent;
};

} // namespace mozilla

#endif // mozilla_IMEContentObserver_h_
