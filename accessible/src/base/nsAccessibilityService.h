/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAccessibilityService_h__
#define __nsAccessibilityService_h__

#include "nsIAccessibilityService.h"

#include "a11yGeneric.h"
#include "nsAccDocManager.h"

#include "mozilla/a11y/FocusManager.h"

#include "nsIObserver.h"

class nsImageFrame;
class nsITreeView;

namespace mozilla {
namespace a11y {

/**
 * Return focus manager.
 */
FocusManager* FocusMgr();

enum EPlatformDisabledState {
  ePlatformIsForceEnabled = -1,
  ePlatformIsEnabled = 0,
  ePlatformIsDisabled = 1
};

/**
 * Return the platform disabled state.
 */
EPlatformDisabledState PlatformDisabledState();

#ifdef MOZ_ACCESSIBILITY_ATK
/**
 * Perform initialization that should be done as soon as possible, in order
 * to minimize startup time.
 * XXX: this function and the next defined in ApplicationAccessibleWrap.cpp
 */
void PreInit();
#endif

#if defined(MOZ_ACCESSIBILITY_ATK) || defined(XP_MACOSX)
/**
 * Is platform accessibility enabled.
 * Only used on linux with atk and MacOS for now.
 */
bool ShouldA11yBeEnabled();
#endif

} // namespace a11y
} // namespace mozilla

class nsAccessibilityService : public nsAccDocManager,
                               public mozilla::a11y::FocusManager,
                               public nsIAccessibilityService,
                               public nsIObserver
{
public:
  virtual ~nsAccessibilityService();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLERETRIEVAL
  NS_DECL_NSIOBSERVER

  // nsIAccessibilityService
  virtual Accessible* GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                bool aCanCreate);
  already_AddRefed<Accessible>
    CreateHTMLButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLBRAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLCanvasAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLCaptionAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLCheckboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLComboboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLFileInputAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLGroupboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLHRAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLImageAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLImageMapAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLLabelAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLLIAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLListboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLMediaAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLObjectFrameAccessible(nsObjectFrame* aFrame, nsIContent* aContent,
                                    nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLRadioButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLTableAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLTableCellAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLTableRowAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateTextLeafAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHTMLTextFieldAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateHyperTextAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<Accessible>
    CreateOuterDocAccessible(nsIContent* aContent, nsIPresShell* aPresShell);

  /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  virtual Accessible* AddNativeRootAccessible(void* aAtkAccessible);
  virtual void RemoveNativeRootAccessible(Accessible* aRootAccessible);

  /**
   * Notification used to update the accessible tree when new content is
   * inserted.
   */
  void ContentRangeInserted(nsIPresShell* aPresShell, nsIContent* aContainer,
                            nsIContent* aStartChild, nsIContent* aEndChild);

  /**
   * Notification used to update the accessible tree when content is removed.
   */
  void ContentRemoved(nsIPresShell* aPresShell, nsIContent* aContainer,
                      nsIContent* aChild);

  virtual void UpdateText(nsIPresShell* aPresShell, nsIContent* aContent);

  /**
   * Update XUL:tree accessible tree when treeview is changed.
   */
  void TreeViewChanged(nsIPresShell* aPresShell, nsIContent* aContent,
                       nsITreeView* aView);

  /**
   * Update list bullet accessible.
   */
  virtual void UpdateListBullet(nsIPresShell* aPresShell,
                                nsIContent* aHTMLListItemContent,
                                bool aHasBullet);

  /**
   * Update the image map.
   */
  void UpdateImageMap(nsImageFrame* aImageFrame);

  /**
   * Notify accessibility that anchor jump has been accomplished to the given
   * target. Used by layout.
   */
  void NotifyOfAnchorJumpTo(nsIContent *aTarget);

  /**
   * Notify that presshell is activated.
   */
  virtual void PresShellActivated(nsIPresShell* aPresShell);

  /**
   * Recreate an accessible for the given content node in the presshell.
   */
  void RecreateAccessible(nsIPresShell* aPresShell, nsIContent* aContent);

  virtual void FireAccessibleEvent(PRUint32 aEvent, Accessible* aTarget);

  // nsAccessibiltiyService

  /**
   * Return true if accessibility service has been shutdown.
   */
  static bool IsShutdown() { return gIsShutdown; }

  /**
   * Return an accessible for the given DOM node from the cache or create new
   * one.
   *
   * @param  aNode             [in] the given node
   * @param  aDoc              [in] the doc accessible of the node
   * @param  aIsSubtreeHidden  [out, optional] indicates whether the node's
   *                             frame and its subtree is hidden
   */
  Accessible* GetOrCreateAccessible(nsINode* aNode, DocAccessible* aDoc,
                                    bool* aIsSubtreeHidden = nsnull);

private:
  // nsAccessibilityService creation is controlled by friend
  // NS_GetAccessibilityService, keep constructors private.
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
   * Create accessible for the element implementing nsIAccessibleProvider
   * interface.
   */
  already_AddRefed<Accessible>
    CreateAccessibleByType(nsIContent* aContent, DocAccessible* aDoc);

  /**
   * Create accessible for HTML node by tag name.
   */
  already_AddRefed<Accessible>
    CreateHTMLAccessibleByMarkup(nsIFrame* aFrame, nsIContent* aContent,
                                 DocAccessible* aDoc);

  /**
   * Create accessible if parent is a deck frame.
   */
  already_AddRefed<Accessible>
    CreateAccessibleForDeckChild(nsIFrame* aFrame, nsIContent* aContent,
                                 DocAccessible* aDoc);

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
   * Indicates whether accessibility service was shutdown.
   */
  static bool gIsShutdown;

  /**
   * Does this content node have a universal ARIA property set on it?
   * A universal ARIA property is one that can be defined on any element even if there is no role.
   *
   * @param aContent The content node to test
   * @return true if there is a universal ARIA property set on the node
   */
  bool HasUniversalAriaProperty(nsIContent *aContent);

  friend nsAccessibilityService* GetAccService();
  friend mozilla::a11y::FocusManager* mozilla::a11y::FocusMgr();

  friend nsresult NS_GetAccessibilityService(nsIAccessibilityService** aResult);
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
 * Map nsIAccessibleEvents constants to strings. Used by
 * nsIAccessibleRetrieval::getStringEventType() method.
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
  "virtual cursor changed"                   // EVENT_VIRTUALCURSOR_CHANGED
};

/**
 * Map nsIAccessibleRelation constants to strings. Used by
 * nsIAccessibleRetrieval::getStringRelationType() method.
 */
static const char kRelationTypeNames[][20] = {
  "unknown",             // RELATION_NUL
  "controlled by",       // RELATION_CONTROLLED_BY
  "controller for",      // RELATION_CONTROLLER_FOR
  "label for",           // RELATION_LABEL_FOR
  "labelled by",         // RELATION_LABELLED_BY
  "member of",           // RELATION_MEMBER_OF
  "node child of",       // RELATION_NODE_CHILD_OF
  "flows to",            // RELATION_FLOWS_TO
  "flows from",          // RELATION_FLOWS_FROM
  "subwindow of",        // RELATION_SUBWINDOW_OF
  "embeds",              // RELATION_EMBEDS
  "embedded by",         // RELATION_EMBEDDED_BY
  "popup for",           // RELATION_POPUP_FOR
  "parent window of",    // RELATION_PARENT_WINDOW_OF
  "described by",        // RELATION_DESCRIBED_BY
  "description for",     // RELATION_DESCRIPTION_FOR
  "default button"       // RELATION_DEFAULT_BUTTON
};

#endif /* __nsIAccessibilityService_h__ */

