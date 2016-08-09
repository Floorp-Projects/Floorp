/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAccessibilityService_h__
#define __nsAccessibilityService_h__

#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/FocusManager.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/SelectionManager.h"
#include "mozilla/Preferences.h"

#include "nsIObserver.h"
#include "nsIAccessibleEvent.h"
#include "nsIEventListenerService.h"
#include "xpcAccessibilityService.h"

class nsImageFrame;
class nsIArray;
class nsIPersistentProperties;
class nsPluginFrame;
class nsITreeView;

namespace mozilla {
namespace a11y {

class ApplicationAccessible;
class xpcAccessibleApplication;

/**
 * Return focus manager.
 */
FocusManager* FocusMgr();

/**
 * Return selection manager.
 */
SelectionManager* SelectionMgr();

/**
 * Returns the application accessible.
 */
ApplicationAccessible* ApplicationAcc();
xpcAccessibleApplication* XPCApplicationAcc();

typedef Accessible* (New_Accessible)(nsIContent* aContent, Accessible* aContext);

struct MarkupAttrInfo {
  nsIAtom** name;
  nsIAtom** value;

  nsIAtom** DOMAttrName;
  nsIAtom** DOMAttrValue;
};

struct MarkupMapInfo {
  nsIAtom** tag;
  New_Accessible* new_func;
  a11y::role role;
  MarkupAttrInfo attrs[4];
};

} // namespace a11y
} // namespace mozilla

class nsAccessibilityService final : public mozilla::a11y::DocManager,
                                     public mozilla::a11y::FocusManager,
                                     public mozilla::a11y::SelectionManager,
                                     public nsIListenerChangeListener,
                                     public nsIObserver
{
public:
  typedef mozilla::a11y::Accessible Accessible;
  typedef mozilla::a11y::DocAccessible DocAccessible;

  // nsIListenerChangeListener
  NS_IMETHOD ListenersChanged(nsIArray* aEventChanges) override;

protected:
  ~nsAccessibilityService();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  Accessible* GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                        bool aCanCreate);
  already_AddRefed<Accessible>
    CreatePluginAccessible(nsPluginFrame* aFrame, nsIContent* aContent,
                           Accessible* aContext);

  /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  Accessible* AddNativeRootAccessible(void* aAtkAccessible);
  void RemoveNativeRootAccessible(Accessible* aRootAccessible);

  bool HasAccessible(nsIDOMNode* aDOMNode);

  /**
   * Get a string equivalent for an accessilbe role value.
   */
  void GetStringRole(uint32_t aRole, nsAString& aString);

  /**
   * Get a string equivalent for an accessible state/extra state.
   */
  void GetStringStates(uint32_t aState, uint32_t aExtraState,
                       nsISupports **aStringStates);

  /**
   * Get a string equivalent for an accessible event value.
   */
  void GetStringEventType(uint32_t aEventType, nsAString& aString);

  /**
   * Get a string equivalent for an accessible relation type.
   */
  void GetStringRelationType(uint32_t aRelationType, nsAString& aString);

  // nsAccesibilityService
  /**
   * Notification used to update the accessible tree when deck panel is
   * switched.
   */
  void DeckPanelSwitched(nsIPresShell* aPresShell, nsIContent* aDeckNode,
                         nsIFrame* aPrevBoxFrame, nsIFrame* aCurrentBoxFrame);

  /**
   * Notification used to update the accessible tree when new content is
   * inserted.
   */
  void ContentRangeInserted(nsIPresShell* aPresShell, nsIContent* aContainer,
                            nsIContent* aStartChild, nsIContent* aEndChild);

  /**
   * Notification used to update the accessible tree when content is removed.
   */
  void ContentRemoved(nsIPresShell* aPresShell, nsIContent* aChild);

  void UpdateText(nsIPresShell* aPresShell, nsIContent* aContent);

  /**
   * Update XUL:tree accessible tree when treeview is changed.
   */
  void TreeViewChanged(nsIPresShell* aPresShell, nsIContent* aContent,
                       nsITreeView* aView);

  /**
   * Notify of input@type="element" value change.
   */
  void RangeValueChanged(nsIPresShell* aPresShell, nsIContent* aContent);

  /**
   * Update list bullet accessible.
   */
  void UpdateListBullet(nsIPresShell* aPresShell,
                        nsIContent* aHTMLListItemContent,
                        bool aHasBullet);

  /**
   * Update the image map.
   */
  void UpdateImageMap(nsImageFrame* aImageFrame);

  /**
   * Update the label accessible tree when rendered @value is changed.
   */
  void UpdateLabelValue(nsIPresShell* aPresShell, nsIContent* aLabelElm,
                        const nsString& aNewValue);

  /**
   * Notify accessibility that anchor jump has been accomplished to the given
   * target. Used by layout.
   */
  void NotifyOfAnchorJumpTo(nsIContent *aTarget);

  /**
   * Notify that presshell is activated.
   */
  void PresShellActivated(nsIPresShell* aPresShell);

  /**
   * Recreate an accessible for the given content node in the presshell.
   */
  void RecreateAccessible(nsIPresShell* aPresShell, nsIContent* aContent);

  void FireAccessibleEvent(uint32_t aEvent, Accessible* aTarget);

  // nsAccessibiltiyService

  /**
   * Return true if accessibility service has been shutdown.
   */
  static bool IsShutdown() { return gIsShutdown; }

  /**
   * Return true if accessibility service has been initialized by platform.
   */
  static bool IsPlatformCaller() { return gIsPlatformCaller; };

  /**
   * Creates an accessible for the given DOM node.
   *
   * @param  aNode             [in] the given node
   * @param  aContext          [in] context the accessible is created in
   * @param  aIsSubtreeHidden  [out, optional] indicates whether the node's
   *                             frame and its subtree is hidden
   */
  Accessible* CreateAccessible(nsINode* aNode, Accessible* aContext,
                               bool* aIsSubtreeHidden = nullptr);

  mozilla::a11y::role MarkupRole(const nsIContent* aContent) const
  {
    const mozilla::a11y::MarkupMapInfo* markupMap =
      mMarkupMaps.Get(aContent->NodeInfo()->NameAtom());
    return markupMap ? markupMap->role : mozilla::a11y::roles::NOTHING;
  }

  /**
   * Set the object attribute defined by markup for the given element.
   */
  void MarkupAttributes(const nsIContent* aContent,
                        nsIPersistentProperties* aAttributes) const;

private:
  // nsAccessibilityService creation is controlled by friend
  // GetOrCreateAccService, keep constructors private.
  nsAccessibilityService();
  nsAccessibilityService(const nsAccessibilityService&);
  nsAccessibilityService& operator =(const nsAccessibilityService&);

private:
  /**
   * Initialize accessibility service.
   */
  bool Init();

  /**
   * Shutdowns accessibility service.
   */
  void Shutdown();

  /**
   * Create accessible for the element having XBL bindings.
   */
  already_AddRefed<Accessible>
    CreateAccessibleByType(nsIContent* aContent, DocAccessible* aDoc);

  /**
   * Create an accessible whose type depends on the given frame.
   */
  already_AddRefed<Accessible>
    CreateAccessibleByFrameType(nsIFrame* aFrame, nsIContent* aContent,
                                Accessible* aContext);

#ifdef MOZ_XUL
  /**
   * Create accessible for XUL tree element.
   */
  already_AddRefed<Accessible>
    CreateAccessibleForXULTree(nsIContent* aContent, DocAccessible* aDoc);
#endif

  /**
   * Reference for accessibility service instance.
   */
  static nsAccessibilityService* gAccessibilityService;

  /**
   * Reference for application accessible instance.
   */
  static mozilla::a11y::ApplicationAccessible* gApplicationAccessible;
  static mozilla::a11y::xpcAccessibleApplication* gXPCApplicationAccessible;

  /**
   * Indicates whether accessibility service was shutdown.
   */
  static bool gIsShutdown;

  /**
   * Indicates whether accessibility service was initialized by platform.
   */
  static bool gIsPlatformCaller;

  nsDataHashtable<nsPtrHashKey<const nsIAtom>, const mozilla::a11y::MarkupMapInfo*> mMarkupMaps;

  friend nsAccessibilityService* GetAccService();
  friend nsAccessibilityService* GetOrCreateAccService(bool);
  friend bool CanShutdownAccService();
  friend mozilla::a11y::FocusManager* mozilla::a11y::FocusMgr();
  friend mozilla::a11y::SelectionManager* mozilla::a11y::SelectionMgr();
  friend mozilla::a11y::ApplicationAccessible* mozilla::a11y::ApplicationAcc();
  friend mozilla::a11y::xpcAccessibleApplication* mozilla::a11y::XPCApplicationAcc();
  friend class xpcAccessibilityService;
};

/**
 * Return the accessibility service instance. (Handy global function)
 */
inline nsAccessibilityService*
GetAccService()
{
  return nsAccessibilityService::gAccessibilityService;
}

/**
 * Return accessibility service instance; creating one if necessary.
 */
nsAccessibilityService* GetOrCreateAccService(bool aIsPlatformCaller = true);

/**
 * Return a flag indicating if accessibility service can be shutdown.
 */
bool CanShutdownAccService();

/**
 * Return true if we're in a content process and not B2G.
 */
inline bool
IPCAccessibilityActive()
{
#ifdef MOZ_B2G
  return false;
#else
  return XRE_IsContentProcess() &&
    mozilla::Preferences::GetBool("accessibility.ipc_architecture.enabled", true);
#endif
}

/**
 * Map nsIAccessibleEvents constants to strings. Used by
 * nsAccessibilityService::GetStringEventType() method.
 */
static const char kEventTypeNames[][40] = {
  "unknown",                                 //
  "show",                                    // EVENT_SHOW
  "hide",                                    // EVENT_HIDE
  "reorder",                                 // EVENT_REORDER
  "active decendent change",                 // EVENT_ACTIVE_DECENDENT_CHANGED
  "focus",                                   // EVENT_FOCUS
  "state change",                            // EVENT_STATE_CHANGE
  "location change",                         // EVENT_LOCATION_CHANGE
  "name changed",                            // EVENT_NAME_CHANGE
  "description change",                      // EVENT_DESCRIPTION_CHANGE
  "value change",                            // EVENT_VALUE_CHANGE
  "help change",                             // EVENT_HELP_CHANGE
  "default action change",                   // EVENT_DEFACTION_CHANGE
  "action change",                           // EVENT_ACTION_CHANGE
  "accelerator change",                      // EVENT_ACCELERATOR_CHANGE
  "selection",                               // EVENT_SELECTION
  "selection add",                           // EVENT_SELECTION_ADD
  "selection remove",                        // EVENT_SELECTION_REMOVE
  "selection within",                        // EVENT_SELECTION_WITHIN
  "alert",                                   // EVENT_ALERT
  "foreground",                              // EVENT_FOREGROUND
  "menu start",                              // EVENT_MENU_START
  "menu end",                                // EVENT_MENU_END
  "menupopup start",                         // EVENT_MENUPOPUP_START
  "menupopup end",                           // EVENT_MENUPOPUP_END
  "capture start",                           // EVENT_CAPTURE_START
  "capture end",                             // EVENT_CAPTURE_END
  "movesize start",                          // EVENT_MOVESIZE_START
  "movesize end",                            // EVENT_MOVESIZE_END
  "contexthelp start",                       // EVENT_CONTEXTHELP_START
  "contexthelp end",                         // EVENT_CONTEXTHELP_END
  "dragdrop start",                          // EVENT_DRAGDROP_START
  "dragdrop end",                            // EVENT_DRAGDROP_END
  "dialog start",                            // EVENT_DIALOG_START
  "dialog end",                              // EVENT_DIALOG_END
  "scrolling start",                         // EVENT_SCROLLING_START
  "scrolling end",                           // EVENT_SCROLLING_END
  "minimize start",                          // EVENT_MINIMIZE_START
  "minimize end",                            // EVENT_MINIMIZE_END
  "document load complete",                  // EVENT_DOCUMENT_LOAD_COMPLETE
  "document reload",                         // EVENT_DOCUMENT_RELOAD
  "document load stopped",                   // EVENT_DOCUMENT_LOAD_STOPPED
  "document attributes changed",             // EVENT_DOCUMENT_ATTRIBUTES_CHANGED
  "document content changed",                // EVENT_DOCUMENT_CONTENT_CHANGED
  "property changed",                        // EVENT_PROPERTY_CHANGED
  "page changed",                           // EVENT_PAGE_CHANGED
  "text attribute changed",                  // EVENT_TEXT_ATTRIBUTE_CHANGED
  "text caret moved",                        // EVENT_TEXT_CARET_MOVED
  "text changed",                            // EVENT_TEXT_CHANGED
  "text inserted",                           // EVENT_TEXT_INSERTED
  "text removed",                            // EVENT_TEXT_REMOVED
  "text updated",                            // EVENT_TEXT_UPDATED
  "text selection changed",                  // EVENT_TEXT_SELECTION_CHANGED
  "visible data changed",                    // EVENT_VISIBLE_DATA_CHANGED
  "text column changed",                     // EVENT_TEXT_COLUMN_CHANGED
  "section changed",                         // EVENT_SECTION_CHANGED
  "table caption changed",                   // EVENT_TABLE_CAPTION_CHANGED
  "table model changed",                     // EVENT_TABLE_MODEL_CHANGED
  "table summary changed",                   // EVENT_TABLE_SUMMARY_CHANGED
  "table row description changed",           // EVENT_TABLE_ROW_DESCRIPTION_CHANGED
  "table row header changed",                // EVENT_TABLE_ROW_HEADER_CHANGED
  "table row insert",                        // EVENT_TABLE_ROW_INSERT
  "table row delete",                        // EVENT_TABLE_ROW_DELETE
  "table row reorder",                       // EVENT_TABLE_ROW_REORDER
  "table column description changed",        // EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED
  "table column header changed",             // EVENT_TABLE_COLUMN_HEADER_CHANGED
  "table column insert",                     // EVENT_TABLE_COLUMN_INSERT
  "table column delete",                     // EVENT_TABLE_COLUMN_DELETE
  "table column reorder",                    // EVENT_TABLE_COLUMN_REORDER
  "window activate",                         // EVENT_WINDOW_ACTIVATE
  "window create",                           // EVENT_WINDOW_CREATE
  "window deactivate",                       // EVENT_WINDOW_DEACTIVATE
  "window destroy",                          // EVENT_WINDOW_DESTROY
  "window maximize",                         // EVENT_WINDOW_MAXIMIZE
  "window minimize",                         // EVENT_WINDOW_MINIMIZE
  "window resize",                           // EVENT_WINDOW_RESIZE
  "window restore",                          // EVENT_WINDOW_RESTORE
  "hyperlink end index changed",             // EVENT_HYPERLINK_END_INDEX_CHANGED
  "hyperlink number of anchors changed",     // EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED
  "hyperlink selected link changed",         // EVENT_HYPERLINK_SELECTED_LINK_CHANGED
  "hypertext link activated",                // EVENT_HYPERTEXT_LINK_ACTIVATED
  "hypertext link selected",                 // EVENT_HYPERTEXT_LINK_SELECTED
  "hyperlink start index changed",           // EVENT_HYPERLINK_START_INDEX_CHANGED
  "hypertext changed",                       // EVENT_HYPERTEXT_CHANGED
  "hypertext links count changed",           // EVENT_HYPERTEXT_NLINKS_CHANGED
  "object attribute changed",                // EVENT_OBJECT_ATTRIBUTE_CHANGED
  "virtual cursor changed",                   // EVENT_VIRTUALCURSOR_CHANGED
  "text value change",                       // EVENT_TEXT_VALUE_CHANGE
};

#endif
