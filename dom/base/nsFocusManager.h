/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFocusManager_h___
#define nsFocusManager_h___

#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIFocusManager.h"
#include "nsIObserver.h"
#include "nsIWidget.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

#define FOCUSMETHOD_MASK 0xF000
#define FOCUSMETHODANDRING_MASK 0xF0F000

#define FOCUSMANAGER_CONTRACTID "@mozilla.org/focus-manager;1"

class nsIContent;
class nsIDocShellTreeItem;
class nsPIDOMWindow;

struct nsDelayedBlurOrFocusEvent;

/**
 * The focus manager keeps track of where the focus is, that is, the node
 * which receives key events.
 */

class nsFocusManager MOZ_FINAL : public nsIFocusManager,
                                 public nsIObserver,
                                 public nsSupportsWeakReference
{
  typedef mozilla::widget::InputContextAction InputContextAction;

public:

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFocusManager, nsIFocusManager)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIFOCUSMANAGER

  // called to initialize and stop the focus manager at startup and shutdown
  static nsresult Init();
  static void Shutdown();

  /**
   * Retrieve the single focus manager.
   */
  static nsFocusManager* GetFocusManager() { return sInstance; }

  /**
   * A faster version of nsIFocusManager::GetFocusedElement, returning a
   * raw nsIContent pointer (instead of having AddRef-ed nsIDOMElement
   * pointer filled in to an out-parameter).
   */
  nsIContent* GetFocusedContent() { return mFocusedContent; }

  /**
   * Return a focused window. Version of nsIFocusManager::GetFocusedWindow.
   */
  nsPIDOMWindow* GetFocusedWindow() const { return mFocusedWindow; }

  /**
   * Return an active window. Version of nsIFocusManager::GetActiveWindow.
   */
  nsPIDOMWindow* GetActiveWindow() const { return mActiveWindow; }

  /**
   * Called when content has been removed.
   */
  nsresult ContentRemoved(nsIDocument* aDocument, nsIContent* aContent);

  /**
   * Called when mouse button event handling is started and finished.
   */
  already_AddRefed<nsIDocument>
    SetMouseButtonHandlingDocument(nsIDocument* aDocument)
  {
    nsCOMPtr<nsIDocument> handlingDocument = mMouseButtonEventHandlingDocument;
    mMouseButtonEventHandlingDocument = aDocument;
    return handlingDocument.forget();
  }

  /**
   * Update the caret with current mode (whether in caret browsing mode or not).
   */
  void UpdateCaretForCaretBrowsingMode();

  /**
   * Returns the content node that would be focused if aWindow was in an
   * active window. This will traverse down the frame hierarchy, starting at
   * the given window aWindow. Sets aFocusedWindow to the window with the
   * document containing aFocusedContent. If no element is focused,
   * aFocusedWindow may be still be set -- this means that the document is
   * focused but no element within it is focused.
   *
   * aWindow and aFocusedWindow must both be non-null.
   */
  static nsIContent* GetFocusedDescendant(nsPIDOMWindow* aWindow, bool aDeep,
                                          nsPIDOMWindow** aFocusedWindow);

  /**
   * Returns the content node that focus will be redirected to if aContent was
   * focused. This is used for the special case of certain XUL elements such
   * as textboxes which redirect focus to an anonymous child.
   *
   * aContent must be non-null.
   *
   * XXXndeakin this should be removed eventually but I want to do that as
   * followup work.
   */
  static nsIContent* GetRedirectedFocus(nsIContent* aContent);

  /**
   * Returns an InputContextAction cause for aFlags.
   */
  static InputContextAction::Cause GetFocusMoveActionCause(uint32_t aFlags);

  static bool sMouseFocusesFormControl;

  static void MarkUncollectableForCCGeneration(uint32_t aGeneration);
protected:

  nsFocusManager();
  ~nsFocusManager();

  /**
   * Ensure that the widget associated with the currently focused window is
   * focused at the widget level.
   */
  void EnsureCurrentWidgetFocused();

  /**
   * Blur whatever is currently focused and focus aNewContent. aFlags is a
   * bitmask of the flags defined in nsIFocusManager. If aFocusChanged is
   * true, then the focus has actually shifted and the caret position will be
   * updated to the new focus, aNewContent will be scrolled into view (unless
   * a flag disables this) and the focus method for the window will be updated.
   * If aAdjustWidget is false, don't change the widget focus state.
   *
   * All actual focus changes must use this method to do so. (as opposed
   * to those that update the focus in an inactive window for instance).
   */
  void SetFocusInner(nsIContent* aNewContent, int32_t aFlags,
                     bool aFocusChanged, bool aAdjustWidget);

  /**
   * Returns true if aPossibleAncestor is the same as aWindow or an
   * ancestor of aWindow.
   */
  bool IsSameOrAncestor(nsPIDOMWindow* aPossibleAncestor,
                          nsPIDOMWindow* aWindow);

  /**
   * Returns the window that is the lowest common ancestor of both aWindow1
   * and aWindow2, or null if they share no common ancestor.
   */
  already_AddRefed<nsPIDOMWindow> GetCommonAncestor(nsPIDOMWindow* aWindow1,
                                                    nsPIDOMWindow* aWindow2);

  /**
   * When aNewWindow is focused, adjust the ancestors of aNewWindow so that they
   * also have their corresponding frames focused. Thus, one can start at
   * the active top-level window and navigate down the currently focused
   * elements for each frame in the tree to get to aNewWindow.
   */
  void AdjustWindowFocus(nsPIDOMWindow* aNewWindow, bool aCheckPermission);

  /**
   * Returns true if aWindow is visible.
   */
  bool IsWindowVisible(nsPIDOMWindow* aWindow);

  /**
   * Returns true if aContent is a root element and not focusable.
   * I.e., even if aContent is editable root element, this returns true when
   * the document is in designMode.
   *
   * @param aContent must not be null and must be in a document.
   */
  bool IsNonFocusableRoot(nsIContent* aContent);

  /**
   * Checks and returns aContent if it may be focused, another content node if
   * the focus should be retargeted at another node, or null if the node
   * cannot be focused. aFlags are the flags passed to SetFocus and similar
   * methods.
   *
   * An element is focusable if it is in a document, the document isn't in
   * print preview mode and the element has an nsIFrame where the
   * CheckIfFocusable method returns true. For <area> elements, there is no
   * frame, so only the IsFocusable method on the content node must be
   * true.
   */
  nsIContent* CheckIfFocusable(nsIContent* aContent, uint32_t aFlags);

  /**
   * Blurs the currently focused element. Returns false if another element was
   * focused as a result. This would mean that the caller should not proceed
   * with a pending call to Focus. Normally, true would be returned.
   *
   * The currently focused element within aWindowToClear will be cleared.
   * aWindowToClear may be null, which means that no window is cleared. This
   * will be the case, for example, when lowering a window, as we want to fire
   * a blur, but not actually change what element would be focused, so that
   * the same element will be focused again when the window is raised.
   *
   * aAncestorWindowToFocus should be set to the common ancestor of the window
   * that is being blurred and the window that is going to focused, when
   * switching focus to a sibling window.
   *
   * aIsLeavingDocument should be set to true if the document/window is being
   * blurred as well. Document/window blur events will be fired. It should be
   * false if an element is the same document is about to be focused.
   *
   * If aAdjustWidget is false, don't change the widget focus state.
   */
  bool Blur(nsPIDOMWindow* aWindowToClear,
              nsPIDOMWindow* aAncestorWindowToFocus,
              bool aIsLeavingDocument,
              bool aAdjustWidget);

  /**
   * Focus an element in the active window and child frame.
   *
   * aWindow is the window containing the element aContent to focus.
   *
   * aFlags is the flags passed to the various focus methods in
   * nsIFocusManager.
   *
   * aIsNewDocument should be true if a new document is being focused.
   * Document/window focus events will be fired.
   *
   * aFocusChanged should be true if a new content node is being focused, so
   * the focused content will be scrolled into view and the caret position
   * will be updated. If false is passed, then a window is simply being
   * refocused, for instance, due to a window being raised, or a tab is being
   * switched to.
   *
   * If aFocusChanged is true, then the focus has moved to a new location.
   * Otherwise, the focus is just being updated because the window was
   * raised.
   *
   * aWindowRaised should be true if the window is being raised. In this case,
   * command updaters will not be called.
   *
   * If aAdjustWidget is false, don't change the widget focus state.
   */
  void Focus(nsPIDOMWindow* aWindow,
             nsIContent* aContent,
             uint32_t aFlags,
             bool aIsNewDocument,
             bool aFocusChanged,
             bool aWindowRaised,
             bool aAdjustWidget);

  /**
   * Fires a focus or blur event at aTarget.
   *
   * aType should be either NS_FOCUS_CONTENT or NS_BLUR_CONTENT. For blur
   * events, aFocusMethod should normally be non-zero.
   *
   * aWindowRaised should only be true if called from WindowRaised.
   */
  void SendFocusOrBlurEvent(uint32_t aType,
                            nsIPresShell* aPresShell,
                            nsIDocument* aDocument,
                            nsISupports* aTarget,
                            uint32_t aFocusMethod,
                            bool aWindowRaised,
                            bool aIsRefocus = false);

  /**
   * Scrolls aContent into view unless the FLAG_NOSCROLL flag is set.
   */
  void ScrollIntoView(nsIPresShell* aPresShell,
                      nsIContent* aContent,
                      uint32_t aFlags);

  /**
   * Raises the top-level window aWindow at the widget level.
   */
  void RaiseWindow(nsPIDOMWindow* aWindow);

  /**
   * Updates the caret positon and visibility to match the focus.
   *
   * aMoveCaretToFocus should be true to move the caret to aContent.
   *
   * aUpdateVisibility should be true to update whether the caret is
   * visible or not.
   */
  void UpdateCaret(bool aMoveCaretToFocus,
                   bool aUpdateVisibility,
                   nsIContent* aContent);

  /**
   * Helper method to move the caret to the focused element aContent.
   */
  void MoveCaretToFocus(nsIPresShell* aPresShell, nsIContent* aContent);

  /**
   * Makes the caret visible or not, depending on aVisible.
   */
  nsresult SetCaretVisible(nsIPresShell* aPresShell,
                           bool aVisible,
                           nsIContent* aContent);


  // the remaining functions are used for tab key and document-navigation

  /**
   * Retrieves the start and end points of the current selection for
   * aDocument and stores them in aStartContent and aEndContent.
   */
  nsresult GetSelectionLocation(nsIDocument* aDocument,
                                nsIPresShell* aPresShell,
                                nsIContent **aStartContent,
                                nsIContent **aEndContent);

  /**
   * Helper function for MoveFocus which determines the next element
   * to move the focus to and returns it in aNextContent.
   *
   * aWindow is the window to adjust the focus within, and aStart is
   * the element to start navigation from. For tab key navigation,
   * this should be the currently focused element.
   *
   * aType is the type passed to MoveFocus. If aNoParentTraversal is set,
   * navigation is not done to parent documents and iteration returns to the
   * beginning (or end) of the starting document.
   */
  nsresult DetermineElementToMoveFocus(nsPIDOMWindow* aWindow,
                                       nsIContent* aStart,
                                       int32_t aType, bool aNoParentTraversal,
                                       nsIContent** aNextContent);

  /**
   * Retrieve the next tabbable element within a document, using focusability
   * and tabindex to determine the tab order. The element is returned in
   * aResultContent.
   *
   * aRootContent is the root node -- nodes above this will not be examined.
   * Typically this will be the root node of a document, but could also be
   * a popup node.
   *
   * aOriginalStartContent is the content which was originally the starting
   * node, in the case of recursive or looping calls.
   *
   * aStartContent is the starting point for this call of this method.
   * If aStartContent doesn't have visual representation, the next content
   * object, which does have a primary frame, will be used as a start.
   * If that content object is focusable, the method may return it.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex is the current tabindex.
   *
   * aIgnoreTabIndex to ignore the current tabindex and find the element
   * irrespective or the tab index. This will be true when a selection is
   * active, since we just want to focus the next element in tree order
   * from where the selection is. Similarly, if the starting element isn't
   * focusable, since it doesn't really have a defined tab index.
   */
  nsresult GetNextTabbableContent(nsIPresShell* aPresShell,
                                  nsIContent* aRootContent,
                                  nsIContent* aOriginalStartContent,
                                  nsIContent* aStartContent,
                                  bool aForward,
                                  int32_t aCurrentTabIndex,
                                  bool aIgnoreTabIndex,
                                  nsIContent** aResultContent);

  /**
   * Get the next tabbable image map area and returns it.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex is the current tabindex.
   *
   * aImageContent is the image.
   *
   * aStartContent is the current image map area.
   */
  nsIContent* GetNextTabbableMapArea(bool aForward,
                                     int32_t aCurrentTabIndex,
                                     nsIContent* aImageContent,
                                     nsIContent* aStartContent);

  /**
   * Return the next valid tabindex value after aCurrentTabIndex, if aForward
   * is true, or the previous tabindex value if aForward is false. aParent is
   * the node from which to start looking for tab indicies.
   */
  int32_t GetNextTabIndex(nsIContent* aParent,
                          int32_t aCurrentTabIndex,
                          bool aForward);

  /**
   * Retrieves and returns the root node from aDocument to be focused. Will
   * return null if the root node cannot be focused. There are several reasons
   * for this:
   *
   * - if aIsForDocNavigation is true, and aWindow is in an <iframe>.
   * - if aIsForDocNavigation is false, and aWindow is a chrome shell.
   * - if aCheckVisibility is true and the aWindow is not visible.
   * - if aDocument is a frameset document.
   */
  nsIContent* GetRootForFocus(nsPIDOMWindow* aWindow,
                              nsIDocument* aDocument,
                              bool aIsForDocNavigation,
                              bool aCheckVisibility);

  /**
   * Get the last docshell child of aItem and return it in aResult.
   */
  void GetLastDocShell(nsIDocShellTreeItem* aItem,
                       nsIDocShellTreeItem** aResult);

  /**
   * Get the next docshell child of aItem and return it in aResult.
   */
  void GetNextDocShell(nsIDocShellTreeItem* aItem,
                       nsIDocShellTreeItem** aResult);

  /**
   * Get the previous docshell child of aItem and return it in aResult.
   */
  void GetPreviousDocShell(nsIDocShellTreeItem* aItem,
                           nsIDocShellTreeItem** aResult);

  /**
   * Determine the first panel with focusable content in document tab order
   * from the given document. aForward indicates the direction to scan. If
   * aCurrentPopup is set to a panel, the next or previous popup after
   * aCurrentPopup after it is used. If aCurrentPopup is null, then the first
   * or last popup is used. If a panel has no focusable content, it is skipped.
   * Null is returned if no panel is open or no open panel contains a focusable
   * element.
   */
  nsIContent* GetNextTabbablePanel(nsIDocument* aDocument, nsIFrame* aCurrentPopup, bool aForward);

  /**
   * Get the tabbable next document from aStartContent or, if null, the
   * currently focused frame if aForward is true, or the previously tabbable
   * document if aForward is false. If this document is a chrome or frameset
   * document, returns the first focusable element within this document,
   * otherwise, returns the root node of the document.
   *
   *
   * Panels with focusable content are also placed in the cycling order, just
   * after the document containing that panel.
   *
   * This method would be used for document navigation, which is typically
   * invoked by pressing F6.
   */
  nsIContent* GetNextTabbableDocument(nsIContent* aStartContent, bool aForward);

  /**
   * Retreives a focusable element within the current selection of aWindow.
   * Currently, this only detects links.
   *  
   * This is used when MoveFocus is called with a type of MOVEFOCUS_CARET,
   * which is used, for example, to focus links as the caret is moved over
   * them.
   */
  void GetFocusInSelection(nsPIDOMWindow* aWindow,
                           nsIContent* aStartSelection,
                           nsIContent* aEndSelection,
                           nsIContent** aFocusedContent);

private:
  // Notify that the focus state of aContent has changed.  Note that
  // we need to pass in whether the window should show a focus ring
  // before the SetFocusedNode call on it happened when losing focus
  // and after the SetFocusedNode call when gaining focus, which is
  // why that information needs to be an explicit argument instead of
  // just passing in the window and asking it whether it should show
  // focus rings: in the losing focus case that information could be
  // wrong..
  static void NotifyFocusStateChange(nsIContent* aContent,
                                     bool aWindowShouldShowFocusRing,
                                     bool aGettingFocus);

  void SetFocusedWindowInternal(nsPIDOMWindow* aWindow);

  // the currently active and front-most top-most window
  nsCOMPtr<nsPIDOMWindow> mActiveWindow;

  // the child or top-level window that is currently focused. This window will
  // either be the same window as mActiveWindow or a descendant of it.
  // Except during shutdown use SetFocusedWindowInternal to set mFocusedWindow!
  nsCOMPtr<nsPIDOMWindow> mFocusedWindow;

  // the currently focused content, which is always inside mFocusedWindow. This
  // is a cached copy of the mFocusedWindow's current content. This may be null
  // if no content is focused.
  nsCOMPtr<nsIContent> mFocusedContent;

  // these fields store a content node temporarily while it is being focused
  // or blurred to ensure that a recursive call doesn't refire the same event.
  // They will always be cleared afterwards.
  nsCOMPtr<nsIContent> mFirstBlurEvent;
  nsCOMPtr<nsIContent> mFirstFocusEvent;

  // keep track of a window while it is being lowered
  nsCOMPtr<nsPIDOMWindow> mWindowBeingLowered;

  // synchronized actions cannot be interrupted with events, so queue these up
  // and fire them later.
  nsTArray<nsDelayedBlurOrFocusEvent> mDelayedBlurFocusEvents;

  // A document which is handling a mouse button event.
  // When a mouse down event process is finished, ESM sets focus to the target
  // content if it's not consumed.  Therefore, while DOM event handlers are
  // handling mouse down events or preceding mosue down event is consumed but
  // handling mouse up events, they should be able to steal focus from any
  // elements even if focus is in chrome content.  So, if this isn't nullptr
  // and the caller can access the document node, the caller should succeed in
  // moving focus.
  nsCOMPtr<nsIDocument> mMouseButtonEventHandlingDocument;

  static bool sTestMode;

  // the single focus manager
  static nsFocusManager* sInstance;
};

nsresult
NS_NewFocusManager(nsIFocusManager** aResult);

#endif
