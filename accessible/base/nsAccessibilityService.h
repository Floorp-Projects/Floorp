/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAccessibilityService_h__
#define __nsAccessibilityService_h__

#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/FocusManager.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/SelectionManager.h"
#include "mozilla/Preferences.h"

#include "nsIContent.h"
#include "nsIObserver.h"
#include "nsIAccessibleEvent.h"
#include "nsIEventListenerService.h"
#include "nsXULAppAPI.h"
#include "xpcAccessibilityService.h"

class nsImageFrame;
class nsIArray;
class nsITreeView;

namespace mozilla {

class PresShell;
class Monitor;
namespace dom {
class DOMStringList;
class Element;
}  // namespace dom

namespace a11y {

class AccAttributes;
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

typedef LocalAccessible*(New_Accessible)(mozilla::dom::Element* aElement,
                                         LocalAccessible* aContext);

// These fields are not `nsStaticAtom* const` because MSVC doesn't like it.
struct MarkupAttrInfo {
  nsStaticAtom* name;
  nsStaticAtom* value;

  nsStaticAtom* DOMAttrName;
  nsStaticAtom* DOMAttrValue;
};

struct MarkupMapInfo {
  const nsStaticAtom* const tag;
  New_Accessible* new_func;
  a11y::role role;
  MarkupAttrInfo attrs[4];
};

struct XULMarkupMapInfo {
  const nsStaticAtom* const tag;
  New_Accessible* new_func;
};

/**
 * PREF_ACCESSIBILITY_FORCE_DISABLED preference change callback.
 */
void PrefChanged(const char* aPref, void* aClosure);

/**
 * Read and normalize PREF_ACCESSIBILITY_FORCE_DISABLED preference.
 */
EPlatformDisabledState ReadPlatformDisabledState();

}  // namespace a11y
}  // namespace mozilla

class nsAccessibilityService final : public mozilla::a11y::DocManager,
                                     public mozilla::a11y::FocusManager,
                                     public mozilla::a11y::SelectionManager,
                                     public nsIListenerChangeListener,
                                     public nsIObserver {
 public:
  typedef mozilla::a11y::LocalAccessible LocalAccessible;
  typedef mozilla::a11y::DocAccessible DocAccessible;

  // nsIListenerChangeListener
  NS_IMETHOD ListenersChanged(nsIArray* aEventChanges) override;

 protected:
  ~nsAccessibilityService();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  LocalAccessible* GetRootDocumentAccessible(mozilla::PresShell* aPresShell,
                                             bool aCanCreate);

  /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  LocalAccessible* AddNativeRootAccessible(void* aAtkAccessible);
  void RemoveNativeRootAccessible(LocalAccessible* aRootAccessible);

  bool HasAccessible(nsINode* aDOMNode);

  /**
   * Get a string equivalent for an accessible role value.
   */
  void GetStringRole(uint32_t aRole, nsAString& aString);

  /**
   * Get a string equivalent for an accessible state/extra state.
   */
  already_AddRefed<mozilla::dom::DOMStringList> GetStringStates(
      uint64_t aStates) const;
  void GetStringStates(uint32_t aState, uint32_t aExtraState,
                       nsISupports** aStringStates);

  /**
   * Get a string equivalent for an accessible event value.
   */
  void GetStringEventType(uint32_t aEventType, nsAString& aString);

  /**
   * Get a string equivalent for an accessible event value.
   */
  void GetStringEventType(uint32_t aEventType, nsACString& aString);

  /**
   * Get a string equivalent for an accessible relation type.
   */
  void GetStringRelationType(uint32_t aRelationType, nsAString& aString);

  // nsAccesibilityService
  /**
   * Notification used to update the accessible tree when deck panel is
   * switched.
   */
  void DeckPanelSwitched(mozilla::PresShell* aPresShell, nsIContent* aDeckNode,
                         nsIFrame* aPrevBoxFrame, nsIFrame* aCurrentBoxFrame);

  /**
   * Notification used to update the accessible tree when new content is
   * inserted.
   */
  void ContentRangeInserted(mozilla::PresShell* aPresShell,
                            nsIContent* aStartChild, nsIContent* aEndChild);

  /**
   * Triggers a re-evaluation of the a11y tree of aContent after the next
   * refresh. This is important because whether we create accessibles may
   * depend on the frame tree / style.
   */
  void ScheduleAccessibilitySubtreeUpdate(mozilla::PresShell* aPresShell,
                                          nsIContent* aStartChild);

  /**
   * Notification used to update the accessible tree when content is removed.
   */
  void ContentRemoved(mozilla::PresShell* aPresShell, nsIContent* aChild);

  /**
   * Notification used to invalidate the isLayoutTable cache.
   */
  void TableLayoutGuessMaybeChanged(mozilla::PresShell* aPresShell,
                                    nsIContent* aContent);

  /**
   * Notifies when a combobox <option> text or label changes.
   */
  void ComboboxOptionMaybeChanged(mozilla::PresShell*,
                                  nsIContent* aMutatingNode);

  void UpdateText(mozilla::PresShell* aPresShell, nsIContent* aContent);

  /**
   * Update XUL:tree accessible tree when treeview is changed.
   */
  void TreeViewChanged(mozilla::PresShell* aPresShell, nsIContent* aContent,
                       nsITreeView* aView);

  /**
   * Notify of input@type="element" value change.
   */
  void RangeValueChanged(mozilla::PresShell* aPresShell, nsIContent* aContent);

  /**
   * Update the image map.
   */
  void UpdateImageMap(nsImageFrame* aImageFrame);

  /**
   * Update the label accessible tree when rendered @value is changed.
   */
  void UpdateLabelValue(mozilla::PresShell* aPresShell, nsIContent* aLabelElm,
                        const nsString& aNewValue);

  /**
   * Notify accessibility that anchor jump has been accomplished to the given
   * target. Used by layout.
   */
  void NotifyOfAnchorJumpTo(nsIContent* aTarget);

  /**
   * Notify that presshell is activated.
   */
  void PresShellActivated(mozilla::PresShell* aPresShell);

  /**
   * Recreate an accessible for the given content node in the presshell.
   */
  void RecreateAccessible(mozilla::PresShell* aPresShell, nsIContent* aContent);

  void FireAccessibleEvent(uint32_t aEvent, LocalAccessible* aTarget);

  void NotifyOfPossibleBoundsChange(mozilla::PresShell* aPresShell,
                                    nsIContent* aContent);

  void NotifyOfComputedStyleChange(mozilla::PresShell* aPresShell,
                                   nsIContent* aContent);

  void NotifyOfResolutionChange(mozilla::PresShell* aPresShell,
                                float aResolution);

  void NotifyOfDevPixelRatioChange(mozilla::PresShell* aPresShell,
                                   int32_t aAppUnitsPerDevPixel);

  // nsAccessibiltiyService

  /**
   * Return true if accessibility service has been shutdown.
   */
  static bool IsShutdown() { return gConsumers == 0; };

  /**
   * Creates an accessible for the given DOM node.
   *
   * @param  aNode             [in] the given node
   * @param  aContext          [in] context the accessible is created in
   * @param  aIsSubtreeHidden  [out, optional] indicates whether the node's
   *                             frame and its subtree is hidden
   */
  LocalAccessible* CreateAccessible(nsINode* aNode, LocalAccessible* aContext,
                                    bool* aIsSubtreeHidden = nullptr);

  mozilla::a11y::role MarkupRole(const nsIContent* aContent) const {
    const mozilla::a11y::MarkupMapInfo* markupMap =
        GetMarkupMapInfoForNode(aContent);
    return markupMap ? markupMap->role : mozilla::a11y::roles::NOTHING;
  }

  /**
   * Return the associated value for a given attribute if
   * it appears in the MarkupMap. Otherwise, it returns null.
   */
  nsStaticAtom* MarkupAttribute(const nsIContent* aContent,
                                nsStaticAtom* aAtom) const {
    const mozilla::a11y::MarkupMapInfo* markupMap =
        GetMarkupMapInfoForNode(aContent);
    if (markupMap) {
      for (size_t i = 0; i < mozilla::ArrayLength(markupMap->attrs); i++) {
        const mozilla::a11y::MarkupAttrInfo* info = markupMap->attrs + i;
        if (info->name == aAtom) {
          return info->value;
        }
      }
    }
    return nullptr;
  }

  /**
   * Set the object attribute defined by markup for the given element.
   */
  void MarkupAttributes(const nsIContent* aContent,
                        mozilla::a11y::AccAttributes* aAttributes) const;

  /**
   * A list of possible accessibility service consumers. Accessibility service
   * can only be shut down when there are no remaining consumers.
   *
   * eXPCOM       - accessibility service is used by XPCOM.
   *
   * eMainProcess - accessibility service was started by main process in the
   *                content process.
   *
   * ePlatformAPI - accessibility service is used by the platform api in the
   *                main process.
   */
  enum ServiceConsumer {
    eXPCOM = 1 << 0,
    eMainProcess = 1 << 1,
    ePlatformAPI = 1 << 2,
  };

#if defined(ANDROID)
  static mozilla::Monitor& GetAndroidMonitor();
#endif

 private:
  // nsAccessibilityService creation is controlled by friend
  // GetOrCreateAccService, keep constructors private.
  nsAccessibilityService();
  nsAccessibilityService(const nsAccessibilityService&);
  nsAccessibilityService& operator=(const nsAccessibilityService&);

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
   * Create an accessible whose type depends on the given frame.
   */
  already_AddRefed<LocalAccessible> CreateAccessibleByFrameType(
      nsIFrame* aFrame, nsIContent* aContent, LocalAccessible* aContext);

  /**
   * Notify observers about change of the accessibility service's consumers.
   */
  void NotifyOfConsumersChange();

  /**
   * Get a JSON string representing the accessibility service consumers.
   */
  void GetConsumers(nsAString& aString);

  /**
   * Set accessibility service consumers.
   */
  void SetConsumers(uint32_t aConsumers, bool aNotify = true);

  /**
   * Unset accessibility service consumers.
   */
  void UnsetConsumers(uint32_t aConsumers);

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
   * Contains a set of accessibility service consumers.
   */
  static uint32_t gConsumers;

  using MarkupMap = nsTHashMap<nsPtrHashKey<const nsAtom>,
                               const mozilla::a11y::MarkupMapInfo*>;
  MarkupMap mHTMLMarkupMap;
  MarkupMap mMathMLMarkupMap;

  const mozilla::a11y::MarkupMapInfo* GetMarkupMapInfoForNode(
      const nsIContent* aContent) const {
    if (aContent->IsHTMLElement()) {
      return mHTMLMarkupMap.Get(aContent->NodeInfo()->NameAtom());
    }
    if (aContent->IsMathMLElement()) {
      return mMathMLMarkupMap.Get(aContent->NodeInfo()->NameAtom());
    }
    // This function can be called by MarkupAttribute, etc. which might in turn
    // be called on a XUL, SVG, etc. element. For example, this can happen
    // with nsAccUtils::SetLiveContainerAttributes.
    return nullptr;
  }

  nsTHashMap<nsPtrHashKey<const nsAtom>, const mozilla::a11y::XULMarkupMapInfo*>
      mXULMarkupMap;

  friend nsAccessibilityService* GetAccService();
  friend nsAccessibilityService* GetOrCreateAccService(uint32_t);
  friend void MaybeShutdownAccService(uint32_t);
  friend void mozilla::a11y::PrefChanged(const char*, void*);
  friend mozilla::a11y::FocusManager* mozilla::a11y::FocusMgr();
  friend mozilla::a11y::SelectionManager* mozilla::a11y::SelectionMgr();
  friend mozilla::a11y::ApplicationAccessible* mozilla::a11y::ApplicationAcc();
  friend mozilla::a11y::xpcAccessibleApplication*
  mozilla::a11y::XPCApplicationAcc();
  friend class xpcAccessibilityService;
};

/**
 * Return the accessibility service instance. (Handy global function)
 */
inline nsAccessibilityService* GetAccService() {
  return nsAccessibilityService::gAccessibilityService;
}

/**
 * Return accessibility service instance; creating one if necessary.
 */
nsAccessibilityService* GetOrCreateAccService(
    uint32_t aNewConsumer = nsAccessibilityService::ePlatformAPI);

/**
 * Shutdown accessibility service if needed.
 */
void MaybeShutdownAccService(uint32_t aFormerConsumer);

/**
 * Return true if we're in a content process and not B2G.
 */
inline bool IPCAccessibilityActive() { return XRE_IsContentProcess(); }

/**
 * Map nsIAccessibleEvents constants to strings. Used by
 * nsAccessibilityService::GetStringEventType() method.
 */
static const char kEventTypeNames[][40] = {
    "unknown",                           //
    "show",                              // EVENT_SHOW
    "hide",                              // EVENT_HIDE
    "reorder",                           // EVENT_REORDER
    "active decendent change",           // EVENT_ACTIVE_DECENDENT_CHANGED
    "focus",                             // EVENT_FOCUS
    "state change",                      // EVENT_STATE_CHANGE
    "location change",                   // EVENT_LOCATION_CHANGE
    "name changed",                      // EVENT_NAME_CHANGE
    "description change",                // EVENT_DESCRIPTION_CHANGE
    "value change",                      // EVENT_VALUE_CHANGE
    "help change",                       // EVENT_HELP_CHANGE
    "default action change",             // EVENT_DEFACTION_CHANGE
    "action change",                     // EVENT_ACTION_CHANGE
    "accelerator change",                // EVENT_ACCELERATOR_CHANGE
    "selection",                         // EVENT_SELECTION
    "selection add",                     // EVENT_SELECTION_ADD
    "selection remove",                  // EVENT_SELECTION_REMOVE
    "selection within",                  // EVENT_SELECTION_WITHIN
    "alert",                             // EVENT_ALERT
    "foreground",                        // EVENT_FOREGROUND
    "menu start",                        // EVENT_MENU_START
    "menu end",                          // EVENT_MENU_END
    "menupopup start",                   // EVENT_MENUPOPUP_START
    "menupopup end",                     // EVENT_MENUPOPUP_END
    "capture start",                     // EVENT_CAPTURE_START
    "capture end",                       // EVENT_CAPTURE_END
    "movesize start",                    // EVENT_MOVESIZE_START
    "movesize end",                      // EVENT_MOVESIZE_END
    "contexthelp start",                 // EVENT_CONTEXTHELP_START
    "contexthelp end",                   // EVENT_CONTEXTHELP_END
    "dragdrop start",                    // EVENT_DRAGDROP_START
    "dragdrop end",                      // EVENT_DRAGDROP_END
    "dialog start",                      // EVENT_DIALOG_START
    "dialog end",                        // EVENT_DIALOG_END
    "scrolling start",                   // EVENT_SCROLLING_START
    "scrolling end",                     // EVENT_SCROLLING_END
    "minimize start",                    // EVENT_MINIMIZE_START
    "minimize end",                      // EVENT_MINIMIZE_END
    "document load complete",            // EVENT_DOCUMENT_LOAD_COMPLETE
    "document reload",                   // EVENT_DOCUMENT_RELOAD
    "document load stopped",             // EVENT_DOCUMENT_LOAD_STOPPED
    "document attributes changed",       // EVENT_DOCUMENT_ATTRIBUTES_CHANGED
    "document content changed",          // EVENT_DOCUMENT_CONTENT_CHANGED
    "property changed",                  // EVENT_PROPERTY_CHANGED
    "page changed",                      // EVENT_PAGE_CHANGED
    "text attribute changed",            // EVENT_TEXT_ATTRIBUTE_CHANGED
    "text caret moved",                  // EVENT_TEXT_CARET_MOVED
    "text changed",                      // EVENT_TEXT_CHANGED
    "text inserted",                     // EVENT_TEXT_INSERTED
    "text removed",                      // EVENT_TEXT_REMOVED
    "text updated",                      // EVENT_TEXT_UPDATED
    "text selection changed",            // EVENT_TEXT_SELECTION_CHANGED
    "visible data changed",              // EVENT_VISIBLE_DATA_CHANGED
    "text column changed",               // EVENT_TEXT_COLUMN_CHANGED
    "section changed",                   // EVENT_SECTION_CHANGED
    "table caption changed",             // EVENT_TABLE_CAPTION_CHANGED
    "table model changed",               // EVENT_TABLE_MODEL_CHANGED
    "table summary changed",             // EVENT_TABLE_SUMMARY_CHANGED
    "table row description changed",     // EVENT_TABLE_ROW_DESCRIPTION_CHANGED
    "table row header changed",          // EVENT_TABLE_ROW_HEADER_CHANGED
    "table row insert",                  // EVENT_TABLE_ROW_INSERT
    "table row delete",                  // EVENT_TABLE_ROW_DELETE
    "table row reorder",                 // EVENT_TABLE_ROW_REORDER
    "table column description changed",  // EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED
    "table column header changed",       // EVENT_TABLE_COLUMN_HEADER_CHANGED
    "table column insert",               // EVENT_TABLE_COLUMN_INSERT
    "table column delete",               // EVENT_TABLE_COLUMN_DELETE
    "table column reorder",              // EVENT_TABLE_COLUMN_REORDER
    "window activate",                   // EVENT_WINDOW_ACTIVATE
    "window create",                     // EVENT_WINDOW_CREATE
    "window deactivate",                 // EVENT_WINDOW_DEACTIVATE
    "window destroy",                    // EVENT_WINDOW_DESTROY
    "window maximize",                   // EVENT_WINDOW_MAXIMIZE
    "window minimize",                   // EVENT_WINDOW_MINIMIZE
    "window resize",                     // EVENT_WINDOW_RESIZE
    "window restore",                    // EVENT_WINDOW_RESTORE
    "hyperlink end index changed",       // EVENT_HYPERLINK_END_INDEX_CHANGED
    "hyperlink number of anchors changed",  // EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED
    "hyperlink selected link changed",  // EVENT_HYPERLINK_SELECTED_LINK_CHANGED
    "hypertext link activated",         // EVENT_HYPERTEXT_LINK_ACTIVATED
    "hypertext link selected",          // EVENT_HYPERTEXT_LINK_SELECTED
    "hyperlink start index changed",    // EVENT_HYPERLINK_START_INDEX_CHANGED
    "hypertext changed",                // EVENT_HYPERTEXT_CHANGED
    "hypertext links count changed",    // EVENT_HYPERTEXT_NLINKS_CHANGED
    "object attribute changed",         // EVENT_OBJECT_ATTRIBUTE_CHANGED
    "virtual cursor changed",           // EVENT_VIRTUALCURSOR_CHANGED
    "text value change",                // EVENT_TEXT_VALUE_CHANGE
    "scrolling",                        // EVENT_SCROLLING
    "announcement",                     // EVENT_ANNOUNCEMENT
    "live region added",                // EVENT_LIVE_REGION_ADDED
    "live region removed",              // EVENT_LIVE_REGION_REMOVED
    "table styling changed",            // EVENT_TABLE_STYLING_CHANGED
    "inner reorder",                    // EVENT_INNER_REORDER
};

#endif
