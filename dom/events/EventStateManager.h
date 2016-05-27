/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventStateManager_h_
#define mozilla_EventStateManager_h_

#include "mozilla/EventForwards.h"

#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/TimeStamp.h"
#include "nsIFrame.h"
#include "Units.h"

class nsFrameLoader;
class nsIContent;
class nsIDocument;
class nsIDocShell;
class nsIDocShellTreeItem;
class imgIContainer;
class EnterLeaveDispatcher;
class nsIContentViewer;
class nsIScrollableFrame;
class nsITimer;
class nsPresContext;

namespace mozilla {

class EnterLeaveDispatcher;
class EventStates;
class IMEContentObserver;
class ScrollbarsForWheel;
class WheelTransaction;

namespace dom {
class DataTransfer;
class Element;
class TabParent;
} // namespace dom

class OverOutElementsWrapper final : public nsISupports
{
  ~OverOutElementsWrapper();

public:
  OverOutElementsWrapper();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(OverOutElementsWrapper)

  nsWeakFrame mLastOverFrame;

  nsCOMPtr<nsIContent> mLastOverElement;

  // The last element on which we fired a over event, or null if
  // the last over event we fired has finished processing.
  nsCOMPtr<nsIContent> mFirstOverEventElement;

  // The last element on which we fired a out event, or null if
  // the last out event we fired has finished processing.
  nsCOMPtr<nsIContent> mFirstOutEventElement;
};

class EventStateManager : public nsSupportsWeakReference,
                          public nsIObserver
{
  friend class mozilla::EnterLeaveDispatcher;
  friend class mozilla::ScrollbarsForWheel;
  friend class mozilla::WheelTransaction;

  virtual ~EventStateManager();

public:
  EventStateManager();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult Init();
  nsresult Shutdown();

  /* The PreHandleEvent method is called before event dispatch to either
   * the DOM or frames.  Any processing which must not be prevented or
   * cancelled should occur here.  Any processing which is intended to
   * be conditional based on either DOM or frame processing should occur in
   * PostHandleEvent.  Any centralized event processing which must occur before
   * DOM or frame event handling should occur here as well.
   */
  nsresult PreHandleEvent(nsPresContext* aPresContext,
                          WidgetEvent* aEvent,
                          nsIFrame* aTargetFrame,
                          nsIContent* aTargetContent,
                          nsEventStatus* aStatus);

  /* The PostHandleEvent method should contain all system processing which
   * should occur conditionally based on DOM or frame processing.  It should
   * also contain any centralized event processing which must occur after
   * DOM and frame processing.
   */
  nsresult PostHandleEvent(nsPresContext* aPresContext,
                           WidgetEvent* aEvent,
                           nsIFrame* aTargetFrame,
                           nsEventStatus* aStatus);

  void PostHandleKeyboardEvent(WidgetKeyboardEvent* aKeyboardEvent,
                               nsEventStatus& aStatus,
                               bool dispatchedToContentProcess);

  /**
   * DispatchLegacyMouseScrollEvents() dispatches eLegacyMouseLineOrPageScroll
   * event and eLegacyMousePixelScroll event for compatibility with old Gecko.
   */
  void DispatchLegacyMouseScrollEvents(nsIFrame* aTargetFrame,
                                       WidgetWheelEvent* aEvent,
                                       nsEventStatus* aStatus);

  void NotifyDestroyPresContext(nsPresContext* aPresContext);
  void SetPresContext(nsPresContext* aPresContext);
  void ClearFrameRefs(nsIFrame* aFrame);

  nsIFrame* GetEventTarget();
  already_AddRefed<nsIContent> GetEventTargetContent(WidgetEvent* aEvent);

  /**
   * Notify that the given NS_EVENT_STATE_* bit has changed for this content.
   * @param aContent Content which has changed states
   * @param aState   Corresponding state flags such as NS_EVENT_STATE_FOCUS
   * @return  Whether the content was able to change all states. Returns false
   *                  if a resulting DOM event causes the content node passed in
   *                  to not change states. Note, the frame for the content may
   *                  change as a result of the content state change, because of
   *                  frame reconstructions that may occur, but this does not
   *                  affect the return value.
   */
  bool SetContentState(nsIContent* aContent, EventStates aState);
  void ContentRemoved(nsIDocument* aDocument, nsIContent* aContent);
  bool EventStatusOK(WidgetGUIEvent* aEvent);

  /**
   * EventStateManager stores IMEContentObserver while it's observing contents.
   * Following mehtods are called by IMEContentObserver when it starts to
   * observe or stops observing the content.
   */
  void OnStartToObserveContent(IMEContentObserver* aIMEContentObserver);
  void OnStopObservingContent(IMEContentObserver* aIMEContentObserver);

  /**
   * TryToFlushPendingNotificationsToIME() suggests flushing pending
   * notifications to IME to IMEContentObserver.
   */
  void TryToFlushPendingNotificationsToIME();

  /**
   * Register accesskey on the given element. When accesskey is activated then
   * the element will be notified via nsIContent::PerformAccesskey() method.
   *
   * @param  aContent  the given element
   * @param  aKey      accesskey
   */
  void RegisterAccessKey(nsIContent* aContent, uint32_t aKey);

  /**
   * Unregister accesskey for the given element.
   *
   * @param  aContent  the given element
   * @param  aKey      accesskey
   */
  void UnregisterAccessKey(nsIContent* aContent, uint32_t aKey);

  /**
   * Get accesskey registered on the given element or 0 if there is none.
   *
   * @param  aContent  the given element (must not be null)
   * @return           registered accesskey
   */
  uint32_t GetRegisteredAccessKey(nsIContent* aContent);

  static void GetAccessKeyLabelPrefix(dom::Element* aElement, nsAString& aPrefix);

  bool HandleAccessKey(WidgetKeyboardEvent* aEvent,
                       nsPresContext* aPresContext,
                       nsTArray<uint32_t>& aAccessCharCodes,
                       int32_t aModifierMask,
                       bool aMatchesContentAccessKey)
  {
    return HandleAccessKey(aEvent, aPresContext, aAccessCharCodes,
                           aMatchesContentAccessKey, nullptr,
                           eAccessKeyProcessingNormal, aModifierMask);
  }

  nsresult SetCursor(int32_t aCursor, imgIContainer* aContainer,
                     bool aHaveHotspot, float aHotspotX, float aHotspotY,
                     nsIWidget* aWidget, bool aLockCursor); 

  static void StartHandlingUserInput()
  {
    ++sUserInputEventDepth;
    ++sUserInputCounter;
    if (sUserInputEventDepth == 1) {
      sLatestUserInputStart = sHandlingInputStart = TimeStamp::Now();
    }
  }

  static void StopHandlingUserInput()
  {
    --sUserInputEventDepth;
    if (sUserInputEventDepth == 0) {
      sHandlingInputStart = TimeStamp();
    }
  }

  /**
   * Returns true if the current code is being executed as a result of
   * user input.  This includes anything that is initiated by user,
   * with the exception of page load events or mouse over events. If
   * this method is called from asynchronously executed code, such as
   * during layout reflows, it will return false. If more time has
   * elapsed since the user input than is specified by the
   * dom.event.handling-user-input-time-limit pref (default 1 second),
   * this function also returns false.
   */
  static bool IsHandlingUserInput();

  /**
   * Get the number of user inputs handled since process start. This
   * includes anything that is initiated by user, with the exception
   * of page load events or mouse over events.
   */
  static uint64_t UserInputCount()
  {
    return sUserInputCounter;
  }

  /**
   * Get the timestamp at which the latest user input was handled.
   *
   * Guaranteed to be monotonic. Until the first user input, return
   * the epoch.
   */
  static TimeStamp LatestUserInputStart()
  {
    return sLatestUserInputStart;
  }

  nsPresContext* GetPresContext() { return mPresContext; }

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EventStateManager,
                                           nsIObserver)

  static nsIDocument* sMouseOverDocument;

  static EventStateManager* GetActiveEventStateManager() { return sActiveESM; }

  // Sets aNewESM to be the active event state manager, and
  // if aContent is non-null, marks the object as active.
  static void SetActiveManager(EventStateManager* aNewESM,
                               nsIContent* aContent);

  // Sets the full-screen event state on aElement to aIsFullScreen.
  static void SetFullScreenState(dom::Element* aElement, bool aIsFullScreen);

  static bool IsRemoteTarget(nsIContent* aTarget);

  // Returns true if the given WidgetWheelEvent will resolve to a scroll action.
  static bool WheelEventIsScrollAction(WidgetWheelEvent* aEvent);

  // Returns user-set multipliers for a wheel event.
  static void GetUserPrefsForWheelEvent(WidgetWheelEvent* aEvent,
                                        double* aOutMultiplierX,
                                        double* aOutMultiplierY);

  // Returns whether or not a frame can be vertically scrolled with a mouse
  // wheel (as opposed to, say, a selection or touch scroll).
  static bool CanVerticallyScrollFrameWithWheel(nsIFrame* aFrame);

  // Holds the point in screen coords that a mouse event was dispatched to,
  // before we went into pointer lock mode. This is constantly updated while
  // the pointer is not locked, but we don't update it while the pointer is
  // locked. This is used by dom::Event::GetScreenCoords() to make mouse
  // events' screen coord appear frozen at the last mouse position while
  // the pointer is locked.
  static CSSIntPoint sLastScreenPoint;

  // Holds the point in client coords of the last mouse event. Used by
  // dom::Event::GetClientCoords() to make mouse events' client coords appear
  // frozen at the last mouse position while the pointer is locked.
  static CSSIntPoint sLastClientPoint;

  static bool sIsPointerLocked;
  static nsWeakPtr sPointerLockedElement;
  static nsWeakPtr sPointerLockedDoc;

  /**
   * If the absolute values of mMultiplierX and/or mMultiplierY are equal or
   * larger than this value, the computed scroll amount isn't rounded down to
   * the page width or height.
   */
  enum {
    MIN_MULTIPLIER_VALUE_ALLOWING_OVER_ONE_PAGE_SCROLL = 1000
  };

protected:
  /**
   * Prefs class capsules preference management.
   */
  class Prefs
  {
  public:
    static bool KeyCausesActivation() { return sKeyCausesActivation; }
    static bool ClickHoldContextMenu() { return sClickHoldContextMenu; }
    static int32_t ChromeAccessModifierMask();
    static int32_t ContentAccessModifierMask();

    static void Init();
    static void OnChange(const char* aPrefName, void*);
    static void Shutdown();

  private:
    static bool sKeyCausesActivation;
    static bool sClickHoldContextMenu;
    static int32_t sGenericAccessModifierKey;
    static int32_t sChromeAccessModifierMask;
    static int32_t sContentAccessModifierMask;

    static int32_t GetAccessModifierMask(int32_t aItemType);
  };

  /**
   * Get appropriate access modifier mask for the aDocShell.  Returns -1 if
   * access key isn't available.
   */
  static int32_t GetAccessModifierMaskFor(nsISupports* aDocShell);

  /*
   * If aTargetFrame's widget has a cached cursor value, resets the cursor
   * such that the next call to SetCursor on the widget will force an update
   * of the native cursor. For use in getting puppet widget to update its
   * cursor between mouse exit / enter transitions. This call basically wraps
   * nsIWidget ClearCachedCursor.
   */
  void ClearCachedWidgetCursor(nsIFrame* aTargetFrame);

  void UpdateCursor(nsPresContext* aPresContext,
                    WidgetEvent* aEvent,
                    nsIFrame* aTargetFrame,
                    nsEventStatus* aStatus);
  /**
   * Turn a GUI mouse/pointer event into a mouse/pointer event targeted at the specified
   * content.  This returns the primary frame for the content (or null
   * if it goes away during the event).
   */
  nsIFrame* DispatchMouseOrPointerEvent(WidgetMouseEvent* aMouseEvent,
                                        EventMessage aMessage,
                                        nsIContent* aTargetContent,
                                        nsIContent* aRelatedContent);
  /**
   * Synthesize DOM pointerover and pointerout events
   */
  void GeneratePointerEnterExit(EventMessage aMessage,
                                WidgetMouseEvent* aEvent);
  /**
   * Synthesize DOM and frame mouseover and mouseout events from this
   * MOUSE_MOVE or MOUSE_EXIT event.
   */
  void GenerateMouseEnterExit(WidgetMouseEvent* aMouseEvent);
  /**
   * Tell this ESM and ESMs in parent documents that the mouse is
   * over some content in this document.
   */
  void NotifyMouseOver(WidgetMouseEvent* aMouseEvent,
                       nsIContent* aContent);
  /**
   * Tell this ESM and ESMs in affected child documents that the mouse
   * has exited this document's currently hovered content.
   * @param aMouseEvent the event that triggered the mouseout
   * @param aMovingInto the content node we've moved into.  This is used to set
   *        the relatedTarget for mouseout events.  Also, if it's non-null
   *        NotifyMouseOut will NOT change the current hover content to null;
   *        in that case the caller is responsible for updating hover state.
   */
  void NotifyMouseOut(WidgetMouseEvent* aMouseEvent,
                      nsIContent* aMovingInto);
  void GenerateDragDropEnterExit(nsPresContext* aPresContext,
                                 WidgetDragEvent* aDragEvent);

  /**
   * Return mMouseEnterLeaveHelper or relevant mPointersEnterLeaveHelper elements wrapper.
   * If mPointersEnterLeaveHelper does not contain wrapper for pointerId it create new one
   */
  OverOutElementsWrapper* GetWrapperByEventID(WidgetMouseEvent* aMouseEvent);

  /**
   * Fire the dragenter and dragexit/dragleave events when the mouse moves to a
   * new target.
   *
   * @param aRelatedTarget relatedTarget to set for the event
   * @param aTargetContent target to set for the event
   * @param aTargetFrame target frame for the event
   */
  void FireDragEnterOrExit(nsPresContext* aPresContext,
                           WidgetDragEvent* aDragEvent,
                           EventMessage aMessage,
                           nsIContent* aRelatedTarget,
                           nsIContent* aTargetContent,
                           nsWeakFrame& aTargetFrame);
  /**
   * Update the initial drag session data transfer with any changes that occur
   * on cloned data transfer objects used for events.
   */
  void UpdateDragDataTransfer(WidgetDragEvent* dragEvent);

  nsresult SetClickCount(WidgetMouseEvent* aEvent, nsEventStatus* aStatus);
  nsresult CheckForAndDispatchClick(WidgetMouseEvent* aEvent,
                                    nsEventStatus* aStatus);
  void EnsureDocument(nsPresContext* aPresContext);
  void FlushPendingEvents(nsPresContext* aPresContext);

  /**
   * The phases of HandleAccessKey processing. See below.
   */
  typedef enum {
    eAccessKeyProcessingNormal = 0,
    eAccessKeyProcessingUp,
    eAccessKeyProcessingDown
  } ProcessingAccessKeyState;

  /**
   * Access key handling.  If there is registered content for the accesskey
   * given by the key event and modifier mask then call
   * content.PerformAccesskey(), otherwise call HandleAccessKey() recursively,
   * on descendant docshells first, then on the ancestor (with |aBubbledFrom|
   * set to the docshell associated with |this|), until something matches.
   *
   * @param aEvent the keyboard event triggering the acccess key
   * @param aPresContext the presentation context
   * @param aAccessCharCodes list of charcode candidates
   * @param aMatchesContentAccessKey true if the content accesskey modifier is pressed
   * @param aBubbledFrom is used by an ancestor to avoid calling HandleAccessKey()
   *        on the child the call originally came from, i.e. this is the child
   *        that recursively called us in its Up phase. The initial caller
   *        passes |nullptr| here. This is to avoid an infinite loop.
   * @param aAccessKeyState Normal, Down or Up processing phase (see enums
   *        above). The initial event receiver uses 'normal', then 'down' when
   *        processing children and Up when recursively calling its ancestor.
   * @param aModifierMask modifier mask for the key event
   */
  bool HandleAccessKey(WidgetKeyboardEvent* aEvent,
                       nsPresContext* aPresContext,
                       nsTArray<uint32_t>& aAccessCharCodes,
                       bool aMatchesContentAccessKey,
                       nsIDocShellTreeItem* aBubbledFrom,
                       ProcessingAccessKeyState aAccessKeyState,
                       int32_t aModifierMask);

  bool ExecuteAccessKey(nsTArray<uint32_t>& aAccessCharCodes,
                        bool aIsTrustedEvent);

  //---------------------------------------------
  // DocShell Focus Traversal Methods
  //---------------------------------------------

  nsIContent* GetFocusedContent();
  bool IsShellVisible(nsIDocShell* aShell);

  // These functions are for mousewheel and pixel scrolling

  class WheelPrefs
  {
  public:
    static WheelPrefs* GetInstance();
    static void Shutdown();

    /**
     * ApplyUserPrefsToDelta() overrides the wheel event's delta values with
     * user prefs.
     */
    void ApplyUserPrefsToDelta(WidgetWheelEvent* aEvent);

    /**
     * Returns whether or not ApplyUserPrefsToDelta() would change the delta
     * values of an event.
     */
    void GetUserPrefsForEvent(WidgetWheelEvent* aEvent,
                              double* aOutMultiplierX,
                              double* aOutMultiplierY);

    /**
     * If ApplyUserPrefsToDelta() changed the delta values with customized
     * prefs, the overflowDelta values would be inflated.
     * CancelApplyingUserPrefsFromOverflowDelta() cancels the inflation.
     */
    void CancelApplyingUserPrefsFromOverflowDelta(WidgetWheelEvent* aEvent);

    /**
     * Computes the default action for the aEvent with the prefs.
     */
    enum Action : uint8_t
    {
      ACTION_NONE = 0,
      ACTION_SCROLL,
      ACTION_HISTORY,
      ACTION_ZOOM,
      ACTION_LAST = ACTION_ZOOM,
      // Following actions are used only by internal processing.  So, cannot
      // specified by prefs.
      ACTION_SEND_TO_PLUGIN
    };
    Action ComputeActionFor(WidgetWheelEvent* aEvent);

    /**
     * NeedToComputeLineOrPageDelta() returns if the aEvent needs to be
     * computed the lineOrPageDelta values.
     */
    bool NeedToComputeLineOrPageDelta(WidgetWheelEvent* aEvent);

    /**
     * IsOverOnePageScrollAllowed*() checks whether wheel scroll amount should
     * be rounded down to the page width/height (false) or not (true).
     */
    bool IsOverOnePageScrollAllowedX(WidgetWheelEvent* aEvent);
    bool IsOverOnePageScrollAllowedY(WidgetWheelEvent* aEvent);

    /**
     * WheelEventsEnabledOnPlugins() returns true if user wants to use mouse
     * wheel on plugins.
     */
    static bool WheelEventsEnabledOnPlugins();

  private:
    WheelPrefs();
    ~WheelPrefs();

    static void OnPrefChanged(const char* aPrefName, void* aClosure);

    enum Index
    {
      INDEX_DEFAULT = 0,
      INDEX_ALT,
      INDEX_CONTROL,
      INDEX_META,
      INDEX_SHIFT,
      INDEX_OS,
      COUNT_OF_MULTIPLIERS
    };

    /**
     * GetIndexFor() returns the index of the members which should be used for
     * the aEvent.  When only one modifier key of MODIFIER_ALT,
     * MODIFIER_CONTROL, MODIFIER_META, MODIFIER_SHIFT or MODIFIER_OS is
     * pressed, returns the index for the modifier.  Otherwise, this return the
     * default index which is used at either no modifier key is pressed or
     * two or modifier keys are pressed.
     */
    Index GetIndexFor(WidgetWheelEvent* aEvent);

    /**
     * GetPrefNameBase() returns the base pref name for aEvent.
     * It's decided by GetModifierForPref() which modifier should be used for
     * the aEvent.
     *
     * @param aBasePrefName The result, must be "mousewheel.with_*." or
     *                      "mousewheel.default.".
     */
    void GetBasePrefName(Index aIndex, nsACString& aBasePrefName);

    void Init(Index aIndex);

    void Reset();

    bool mInit[COUNT_OF_MULTIPLIERS];
    double mMultiplierX[COUNT_OF_MULTIPLIERS];
    double mMultiplierY[COUNT_OF_MULTIPLIERS];
    double mMultiplierZ[COUNT_OF_MULTIPLIERS];
    Action mActions[COUNT_OF_MULTIPLIERS];
    /**
     * action values overridden by .override_x pref.
     * If an .override_x value is -1, same as the
     * corresponding mActions value.
     */
    Action mOverriddenActionsX[COUNT_OF_MULTIPLIERS];

    static WheelPrefs* sInstance;

    static bool sWheelEventsEnabledOnPlugins;
  };

  /**
   * DeltaDirection is used for specifying whether the called method should
   * handle vertical delta or horizontal delta.
   * This is clearer than using bool.
   */
  enum DeltaDirection
  {
    DELTA_DIRECTION_X = 0,
    DELTA_DIRECTION_Y
  };

  struct MOZ_STACK_CLASS EventState
  {
    bool mDefaultPrevented;
    bool mDefaultPreventedByContent;

    EventState() :
      mDefaultPrevented(false), mDefaultPreventedByContent(false)
    {
    }
  };

  /**
   * SendLineScrollEvent() dispatches a DOMMouseScroll event for the
   * WidgetWheelEvent.  This method shouldn't be called for non-trusted
   * wheel event because it's not necessary for compatiblity.
   *
   * @param aTargetFrame        The event target of wheel event.
   * @param aEvent              The original Wheel event.
   * @param aState              The event which should be set to the dispatching
   *                            event.  This also returns the dispatched event
   *                            state.
   * @param aDelta              The delta value of the event.
   * @param aDeltaDirection     The X/Y direction of dispatching event.
   */
  void SendLineScrollEvent(nsIFrame* aTargetFrame,
                           WidgetWheelEvent* aEvent,
                           EventState& aState,
                           int32_t aDelta,
                           DeltaDirection aDeltaDirection);

  /**
   * SendPixelScrollEvent() dispatches a MozMousePixelScroll event for the
   * WidgetWheelEvent.  This method shouldn't be called for non-trusted
   * wheel event because it's not necessary for compatiblity.
   *
   * @param aTargetFrame        The event target of wheel event.
   * @param aEvent              The original Wheel event.
   * @param aState              The event which should be set to the dispatching
   *                            event.  This also returns the dispatched event
   *                            state.
   * @param aPixelDelta         The delta value of the event.
   * @param aDeltaDirection     The X/Y direction of dispatching event.
   */
  void SendPixelScrollEvent(nsIFrame* aTargetFrame,
                            WidgetWheelEvent* aEvent,
                            EventState& aState,
                            int32_t aPixelDelta,
                            DeltaDirection aDeltaDirection);

  /**
   * ComputeScrollTarget() returns the scrollable frame which should be
   * scrolled.
   *
   * @param aTargetFrame        The event target of the wheel event.
   * @param aEvent              The handling mouse wheel event.
   * @param aOptions            The options for finding the scroll target.
   *                            Callers should use COMPUTE_*.
   * @return                    The scrollable frame which should be scrolled.
   */
  // These flags are used in ComputeScrollTarget(). Callers should use
  // COMPUTE_*.
  enum
  {
    PREFER_MOUSE_WHEEL_TRANSACTION               = 0x00000001,
    PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS = 0x00000002,
    PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS = 0x00000004,
    START_FROM_PARENT                            = 0x00000008,
    INCLUDE_PLUGIN_AS_TARGET                     = 0x00000010
  };
  enum ComputeScrollTargetOptions
  {
    // At computing scroll target for legacy mouse events, we should return
    // first scrollable element even when it's not scrollable to the direction.
    COMPUTE_LEGACY_MOUSE_SCROLL_EVENT_TARGET     = 0,
    // Default action prefers the scrolled element immediately before if it's
    // still under the mouse cursor.  Otherwise, it prefers the nearest
    // scrollable ancestor which will be scrolled actually.
    COMPUTE_DEFAULT_ACTION_TARGET_EXCEPT_PLUGIN  =
      (PREFER_MOUSE_WHEEL_TRANSACTION |
       PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS |
       PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS),
    // When this is specified, the result may be nsPluginFrame.  In such case,
    // the frame doesn't have nsIScrollableFrame interface.
    COMPUTE_DEFAULT_ACTION_TARGET                =
      (COMPUTE_DEFAULT_ACTION_TARGET_EXCEPT_PLUGIN |
       INCLUDE_PLUGIN_AS_TARGET),
    // Look for the nearest scrollable ancestor which can be scrollable with
    // aEvent.
    COMPUTE_SCROLLABLE_ANCESTOR_ALONG_X_AXIS     =
      (PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS | START_FROM_PARENT),
    COMPUTE_SCROLLABLE_ANCESTOR_ALONG_Y_AXIS     =
      (PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS | START_FROM_PARENT)
  };
  static ComputeScrollTargetOptions RemovePluginFromTarget(
                                      ComputeScrollTargetOptions aOptions)
  {
    switch (aOptions) {
      case COMPUTE_DEFAULT_ACTION_TARGET:
        return COMPUTE_DEFAULT_ACTION_TARGET_EXCEPT_PLUGIN;
      default:
        MOZ_ASSERT(!(aOptions & INCLUDE_PLUGIN_AS_TARGET));
        return aOptions;
    }
  }
  nsIFrame* ComputeScrollTarget(nsIFrame* aTargetFrame,
                                WidgetWheelEvent* aEvent,
                                ComputeScrollTargetOptions aOptions);

  nsIFrame* ComputeScrollTarget(nsIFrame* aTargetFrame,
                                double aDirectionX,
                                double aDirectionY,
                                WidgetWheelEvent* aEvent,
                                ComputeScrollTargetOptions aOptions);

  /**
   * GetScrollAmount() returns the scroll amount in app uints of one line or
   * one page.  If the wheel event scrolls a page, returns the page width and
   * height.  Otherwise, returns line height for both its width and height.
   *
   * @param aScrollableFrame    A frame which will be scrolled by the event.
   *                            The result of ComputeScrollTarget() is
   *                            expected for this value.
   *                            This can be nullptr if there is no scrollable
   *                            frame.  Then, this method uses root frame's
   *                            line height or visible area's width and height.
   */
  nsSize GetScrollAmount(nsPresContext* aPresContext,
                         WidgetWheelEvent* aEvent,
                         nsIScrollableFrame* aScrollableFrame);

  /**
   * DoScrollText() scrolls the scrollable frame for aEvent.
   */
  void DoScrollText(nsIScrollableFrame* aScrollableFrame,
                    WidgetWheelEvent* aEvent);

  void DoScrollHistory(int32_t direction);
  void DoScrollZoom(nsIFrame *aTargetFrame, int32_t adjustment);
  nsresult GetContentViewer(nsIContentViewer** aCv);
  nsresult ChangeTextSize(int32_t change);
  nsresult ChangeFullZoom(int32_t change);

  /**
   * DeltaAccumulator class manages delta values for dispatching DOMMouseScroll
   * event.  If wheel events are caused by pixel scroll only devices or
   * the delta values are customized by prefs, this class stores the delta
   * values and set lineOrPageDelta values.
   */
  class DeltaAccumulator
  {
  public:
    static DeltaAccumulator* GetInstance()
    {
      if (!sInstance) {
        sInstance = new DeltaAccumulator;
      }
      return sInstance;
    }

    static void Shutdown()
    {
      delete sInstance;
      sInstance = nullptr;
    }

    bool IsInTransaction() { return mHandlingDeltaMode != UINT32_MAX; }

    /**
     * InitLineOrPageDelta() stores pixel delta values of WidgetWheelEvents
     * which are caused if it's needed.  And if the accumulated delta becomes a
     * line height, sets lineOrPageDeltaX and lineOrPageDeltaY automatically.
     */
    void InitLineOrPageDelta(nsIFrame* aTargetFrame,
                             EventStateManager* aESM,
                             WidgetWheelEvent* aEvent);

    /**
     * Reset() resets all members.
     */
    void Reset();

    /**
     * ComputeScrollAmountForDefaultAction() computes the default action's
     * scroll amount in device pixels with mPendingScrollAmount*.
     */
    nsIntPoint ComputeScrollAmountForDefaultAction(
                 WidgetWheelEvent* aEvent,
                 const nsIntSize& aScrollAmountInDevPixels);

  private:
    DeltaAccumulator() :
      mX(0.0), mY(0.0), mPendingScrollAmountX(0.0), mPendingScrollAmountY(0.0),
      mHandlingDeltaMode(UINT32_MAX), mIsNoLineOrPageDeltaDevice(false)
    {
    }

    double mX;
    double mY;

    // When default action of a wheel event is scroll but some delta values
    // are ignored because the computed amount values are not integer, the
    // fractional values are saved by these members.
    double mPendingScrollAmountX;
    double mPendingScrollAmountY;

    TimeStamp mLastTime;

    uint32_t mHandlingDeltaMode;
    bool mIsNoLineOrPageDeltaDevice;

    static DeltaAccumulator* sInstance;
  };

  // end mousewheel functions

  /*
   * When a touch gesture is about to start, this function determines what
   * kind of gesture interaction we will want to use, based on what is
   * underneath the initial touch point.
   * Currently it decides between panning (finger scrolling) or dragging
   * the target element, as well as the orientation to trigger panning and
   * display visual boundary feedback. The decision is stored back in aEvent.
   */
  void DecideGestureEvent(WidgetGestureNotifyEvent* aEvent,
                          nsIFrame* targetFrame);

  // routines for the d&d gesture tracking state machine
  void BeginTrackingDragGesture(nsPresContext* aPresContext,
                                WidgetMouseEvent* aDownEvent,
                                nsIFrame* aDownFrame);

  friend class mozilla::dom::TabParent;
  void BeginTrackingRemoteDragGesture(nsIContent* aContent);
  void StopTrackingDragGesture();
  void GenerateDragGesture(nsPresContext* aPresContext,
                           WidgetMouseEvent* aEvent);

  /**
   * Determine which node the drag should be targeted at.
   * This is either the node clicked when there is a selection, or, for HTML,
   * the element with a draggable property set to true.
   *
   * aSelectionTarget - target to check for selection
   * aDataTransfer - data transfer object that will contain the data to drag
   * aSelection - [out] set to the selection to be dragged
   * aTargetNode - [out] the draggable node, or null if there isn't one
   */
  void DetermineDragTargetAndDefaultData(nsPIDOMWindowOuter* aWindow,
                                         nsIContent* aSelectionTarget,
                                         dom::DataTransfer* aDataTransfer,
                                         nsISelection** aSelection,
                                         nsIContent** aTargetNode);

  /*
   * Perform the default handling for the dragstart event and set up a
   * drag for aDataTransfer if it contains any data. Returns true if a drag has
   * started.
   *
   * aDragEvent - the dragstart event
   * aDataTransfer - the data transfer that holds the data to be dragged
   * aDragTarget - the target of the drag
   * aSelection - the selection to be dragged
   */
  bool DoDefaultDragStart(nsPresContext* aPresContext,
                          WidgetDragEvent* aDragEvent,
                          dom::DataTransfer* aDataTransfer,
                          nsIContent* aDragTarget,
                          nsISelection* aSelection);

  bool IsTrackingDragGesture ( ) const { return mGestureDownContent != nullptr; }
  /**
   * Set the fields of aEvent to reflect the mouse position and modifier keys
   * that were set when the user first pressed the mouse button (stored by
   * BeginTrackingDragGesture). aEvent->mWidget must be
   * mCurrentTarget->GetNearestWidget().
   */
  void FillInEventFromGestureDown(WidgetMouseEvent* aEvent);

  nsresult DoContentCommandEvent(WidgetContentCommandEvent* aEvent);
  nsresult DoContentCommandScrollEvent(WidgetContentCommandEvent* aEvent);

  dom::TabParent *GetCrossProcessTarget();
  bool IsTargetCrossProcess(WidgetGUIEvent* aEvent);

  bool DispatchCrossProcessEvent(WidgetEvent* aEvent,
                                 nsFrameLoader* aRemote,
                                 nsEventStatus *aStatus);
  bool HandleCrossProcessEvent(WidgetEvent* aEvent,
                               nsEventStatus* aStatus);

  void ReleaseCurrentIMEContentObserver();

  void HandleQueryContentEvent(WidgetQueryContentEvent* aEvent);

private:
  static inline void DoStateChange(dom::Element* aElement,
                                   EventStates aState, bool aAddState);
  static inline void DoStateChange(nsIContent* aContent, EventStates aState,
                                   bool aAddState);
  static void UpdateAncestorState(nsIContent* aStartNode,
                                  nsIContent* aStopBefore,
                                  EventStates aState,
                                  bool aAddState);
  static void ResetLastOverForContent(const uint32_t& aIdx,
                                      RefPtr<OverOutElementsWrapper>& aChunk,
                                      nsIContent* aClosure);

  int32_t     mLockCursor;
  bool mLastFrameConsumedSetCursor;

  // Last mouse event mRefPoint (the offset from the widget's origin in
  // device pixels) when mouse was locked, used to restore mouse position
  // after unlocking.
  LayoutDeviceIntPoint mPreLockPoint;

  // Stores the mRefPoint of the last synthetic mouse move we dispatched
  // to re-center the mouse when we were pointer locked. If this is (-1,-1) it
  // means we've not recently dispatched a centering event. We use this to
  // detect when we receive the synth event, so we can cancel and not send it
  // to content.
  static LayoutDeviceIntPoint sSynthCenteringPoint;

  nsWeakFrame mCurrentTarget;
  nsCOMPtr<nsIContent> mCurrentTargetContent;
  static nsWeakFrame sLastDragOverFrame;

  // Stores the mRefPoint (the offset from the widget's origin in device
  // pixels) of the last mouse event.
  static LayoutDeviceIntPoint sLastRefPoint;

  // member variables for the d&d gesture state machine
  LayoutDeviceIntPoint mGestureDownPoint; // screen coordinates
  // The content to use as target if we start a d&d (what we drag).
  nsCOMPtr<nsIContent> mGestureDownContent;
  // The content of the frame where the mouse-down event occurred. It's the same
  // as the target in most cases but not always - for example when dragging
  // an <area> of an image map this is the image. (bug 289667)
  nsCOMPtr<nsIContent> mGestureDownFrameOwner;
  // State of keys when the original gesture-down happened
  Modifiers mGestureModifiers;
  uint16_t mGestureDownButtons;

  nsCOMPtr<nsIContent> mLastLeftMouseDownContent;
  nsCOMPtr<nsIContent> mLastLeftMouseDownContentParent;
  nsCOMPtr<nsIContent> mLastMiddleMouseDownContent;
  nsCOMPtr<nsIContent> mLastMiddleMouseDownContentParent;
  nsCOMPtr<nsIContent> mLastRightMouseDownContent;
  nsCOMPtr<nsIContent> mLastRightMouseDownContentParent;

  nsCOMPtr<nsIContent> mActiveContent;
  nsCOMPtr<nsIContent> mHoverContent;
  static nsCOMPtr<nsIContent> sDragOverContent;
  nsCOMPtr<nsIContent> mURLTargetContent;

  nsPresContext* mPresContext;      // Not refcnted
  nsCOMPtr<nsIDocument> mDocument;   // Doesn't necessarily need to be owner

  RefPtr<IMEContentObserver> mIMEContentObserver;

  uint32_t mLClickCount;
  uint32_t mMClickCount;
  uint32_t mRClickCount;

  bool m_haveShutdown;

  // Time at which we began handling user input. Reset to the epoch
  // once we have finished handling user input.
  static TimeStamp sHandlingInputStart;

  // Time at which we began handling the latest user input. Not reset
  // at the end of the input.
  static TimeStamp sLatestUserInputStart;

  RefPtr<OverOutElementsWrapper> mMouseEnterLeaveHelper;
  nsRefPtrHashtable<nsUint32HashKey, OverOutElementsWrapper> mPointersEnterLeaveHelper;

public:
  static nsresult UpdateUserActivityTimer(void);
  // Array for accesskey support
  nsCOMArray<nsIContent> mAccessKeys;

  // The number of user inputs handled since process start. This
  // includes anything that is initiated by user, with the exception
  // of page load events or mouse over events.
  static uint64_t sUserInputCounter;

  // The current depth of user inputs. This includes anything that is
  // initiated by user, with the exception of page load events or
  // mouse over events. Incremented whenever we start handling a user
  // input, decremented when we have finished handling a user
  // input. This depth is *not* reset in case of nested event loops.
  static int32_t sUserInputEventDepth;
  
  static bool sNormalLMouseEventInProcess;

  static EventStateManager* sActiveESM;
  
  static void ClearGlobalActiveContent(EventStateManager* aClearer);

  // Functions used for click hold context menus
  nsCOMPtr<nsITimer> mClickHoldTimer;
  void CreateClickHoldTimer(nsPresContext* aPresContext,
                            nsIFrame* aDownFrame,
                            WidgetGUIEvent* aMouseDownEvent);
  void KillClickHoldTimer();
  void FireContextClick();

  void SetPointerLock(nsIWidget* aWidget, nsIContent* aElement) ;
  static void sClickHoldCallback ( nsITimer* aTimer, void* aESM ) ;
};

/**
 * This class is used while processing real user input. During this time, popups
 * are allowed. For mousedown events, mouse capturing is also permitted.
 */
class AutoHandlingUserInputStatePusher
{
public:
  AutoHandlingUserInputStatePusher(bool aIsHandlingUserInput,
                                   WidgetEvent* aEvent,
                                   nsIDocument* aDocument);
  ~AutoHandlingUserInputStatePusher();

protected:
  bool mIsHandlingUserInput;
  bool mIsMouseDown;
  bool mResetFMMouseButtonHandlingState;

  nsCOMPtr<nsIDocument> mMouseButtonEventHandlingDocument;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nullptr; }
  static void operator delete(void* /*memory*/) {}
};

} // namespace mozilla

// Click and double-click events need to be handled even for content that
// has no frame. This is required for Web compatibility.
#define NS_EVENT_NEEDS_FRAME(event) \
    (!(event)->HasPluginActivationEventMessage() && \
     (event)->mMessage != eMouseClick && \
     (event)->mMessage != eMouseDoubleClick)

#endif // mozilla_EventStateManager_h_
