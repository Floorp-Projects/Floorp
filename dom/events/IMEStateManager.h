/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IMEStateManager_h_
#define mozilla_IMEStateManager_h_

#include "mozilla/EventForwards.h"
#include "nsIWidget.h"

class nsIContent;
class nsIDOMMouseEvent;
class nsIEditor;
class nsINode;
class nsPIDOMWindow;
class nsPresContext;
class nsISelection;

namespace mozilla {

class EventDispatchingCallback;
class IMEContentObserver;
class TextCompositionArray;
class TextComposition;

/**
 * IMEStateManager manages InputContext (e.g., active editor type, IME enabled
 * state and IME open state) of nsIWidget instances, manages IMEContentObserver
 * and provides useful API for IME.
 */

class IMEStateManager
{
  typedef widget::IMEMessage IMEMessage;
  typedef widget::IMEState IMEState;
  typedef widget::InputContext InputContext;
  typedef widget::InputContextAction InputContextAction;

public:
  static void Init();
  static void Shutdown();

  static nsresult OnDestroyPresContext(nsPresContext* aPresContext);
  static nsresult OnRemoveContent(nsPresContext* aPresContext,
                                  nsIContent* aContent);
  /**
   * OnChangeFocus() should be called when focused content is changed or
   * IME enabled state is changed.  If nobody has focus, set both aPresContext
   * and aContent nullptr.  E.g., all windows are deactivated.
   */
  static nsresult OnChangeFocus(nsPresContext* aPresContext,
                                nsIContent* aContent,
                                InputContextAction::Cause aCause);
  static void OnInstalledMenuKeyboardListener(bool aInstalling);

  // These two methods manage focus and selection/text observers.
  // They are separate from OnChangeFocus above because this offers finer
  // control compared to having the two methods incorporated into OnChangeFocus

  // Get the focused editor's selection and root
  static nsresult GetFocusSelectionAndRoot(nsISelection** aSel,
                                           nsIContent** aRoot);
  // This method updates the current IME state.  However, if the enabled state
  // isn't changed by the new state, this method does nothing.
  // Note that this method changes the IME state of the active element in the
  // widget.  So, the caller must have focus.
  static void UpdateIMEState(const IMEState &aNewIMEState,
                             nsIContent* aContent,
                             nsIEditor* aEditor);

  // This method is called when user operates mouse button in focused editor
  // and before the editor handles it.
  // Returns true if IME consumes the event.  Otherwise, false.
  static bool OnMouseButtonEventInEditor(nsPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsIDOMMouseEvent* aMouseEvent);

  // This method is called when user clicked in an editor.
  // aContent must be:
  //   If the editor is for <input> or <textarea>, the element.
  //   If the editor is for contenteditable, the active editinghost.
  //   If the editor is for designMode, nullptr.
  static void OnClickInEditor(nsPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIDOMMouseEvent* aMouseEvent);

  // This method is called when editor actually gets focus.
  // aContent must be:
  //   If the editor is for <input> or <textarea>, the element.
  //   If the editor is for contenteditable, the active editinghost.
  //   If the editor is for designMode, nullptr.
  static void OnFocusInEditor(nsPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIEditor* aEditor);

  /**
   * All composition events must be dispatched via DispatchCompositionEvent()
   * for storing the composition target and ensuring a set of composition
   * events must be fired the stored target.  If the stored composition event
   * target is destroying, this removes the stored composition automatically.
   */
  static void DispatchCompositionEvent(
                nsINode* aEventTargetNode,
                nsPresContext* aPresContext,
                WidgetCompositionEvent* aCompositionEvent,
                nsEventStatus* aStatus,
                EventDispatchingCallback* aCallBack,
                bool aIsSynthesized = false);

  /**
   * This is called when PresShell ignores a composition event due to not safe
   * to dispatch events.
   */
  static void OnCompositionEventDiscarded(
                const WidgetCompositionEvent* aCompositionEvent);

  /**
   * Get TextComposition from widget.
   */
  static already_AddRefed<TextComposition>
    GetTextCompositionFor(nsIWidget* aWidget);

  /**
   * Returns TextComposition instance for the event.
   *
   * @param aGUIEvent Should be a composition event which is being dispatched.
   */
  static already_AddRefed<TextComposition>
    GetTextCompositionFor(WidgetGUIEvent* aGUIEvent);

  /**
   * Send a notification to IME.  It depends on the IME or platform spec what
   * will occur (or not occur).
   */
  static nsresult NotifyIME(IMEMessage aMessage, nsIWidget* aWidget);
  static nsresult NotifyIME(IMEMessage aMessage, nsPresContext* aPresContext);

  static nsINode* GetRootEditableNode(nsPresContext* aPresContext,
                                      nsIContent* aContent);
  static bool IsTestingIME() { return sIsTestingIME; }

protected:
  static nsresult OnChangeFocusInternal(nsPresContext* aPresContext,
                                        nsIContent* aContent,
                                        InputContextAction aAction);
  static void SetIMEState(const IMEState &aState,
                          nsIContent* aContent,
                          nsIWidget* aWidget,
                          InputContextAction aAction);
  static IMEState GetNewIMEState(nsPresContext* aPresContext,
                                 nsIContent* aContent);

  static void EnsureTextCompositionArray();
  static void CreateIMEContentObserver(nsIEditor* aEditor);
  static void DestroyIMEContentObserver();

  static bool IsEditable(nsINode* node);

  static bool IsEditableIMEState(nsIWidget* aWidget);

  static nsIContent*    sContent;
  static nsPresContext* sPresContext;
  static bool           sInstalledMenuKeyboardListener;
  static bool           sIsTestingIME;
  static bool           sIsGettingNewIMEState;

  class MOZ_STACK_CLASS GettingNewIMEStateBlocker MOZ_FINAL
  {
  public:
    GettingNewIMEStateBlocker()
      : mOldValue(IMEStateManager::sIsGettingNewIMEState)
    {
      IMEStateManager::sIsGettingNewIMEState = true;
    }
    ~GettingNewIMEStateBlocker()
    {
      IMEStateManager::sIsGettingNewIMEState = mOldValue;
    }
  private:
    bool mOldValue;
  };

  static IMEContentObserver* sActiveIMEContentObserver;

  // All active compositions in the process are stored by this array.
  // When you get an item of this array and use it, please be careful.
  // The instances in this array can be destroyed automatically if you do
  // something to cause committing or canceling the composition.
  static TextCompositionArray* sTextCompositions;
};

} // namespace mozilla

#endif // mozilla_IMEStateManager_h_
