/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAccUtils_h_
#define nsAccUtils_h_

#include "mozilla/a11y/LocalAccessible.h"
#include "mozilla/a11y/DocManager.h"

#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"

#include "nsIDocShell.h"
#include "nsPoint.h"

namespace mozilla {

class PresShell;

namespace dom {
class Element;
}

namespace a11y {

class HyperTextAccessible;
class DocAccessible;
class Attribute;

class nsAccUtils {
 public:
  /**
   * Set group attributes ('level', 'setsize', 'posinset').
   */
  static void SetAccGroupAttrs(AccAttributes* aAttributes, int32_t aLevel,
                               int32_t aSetSize, int32_t aPosInSet);

  /**
   * Compute group level for nsIDOMXULContainerItemElement node.
   */
  static int32_t GetLevelForXULContainerItem(nsIContent* aContent);

  /**
   * Set container-foo live region attributes for the given node.
   *
   * @param aAttributes    where to store the attributes
   * @param aStartAcc  Accessible to start from
   */
  static void SetLiveContainerAttributes(AccAttributes* aAttributes,
                                         Accessible* aStartAcc);

  /**
   * Any ARIA property of type boolean or NMTOKEN is undefined if the ARIA
   * property is not present, or is "" or "undefined". Do not call
   * this method for properties of type string, decimal, IDREF or IDREFS.
   *
   * Return true if the ARIA property is defined, otherwise false
   */
  static bool HasDefinedARIAToken(nsIContent* aContent, nsAtom* aAtom);

  /**
   * Return atomic value of ARIA attribute of boolean or NMTOKEN type.
   */
  static nsStaticAtom* GetARIAToken(mozilla::dom::Element* aElement,
                                    nsAtom* aAttr);

  /**
   * If the given ARIA attribute has a specific known token value, return it.
   * If the specification demands for a fallback value for unknown attribute
   * values, return that. For all others, return a nullptr.
   */
  static nsStaticAtom* NormalizeARIAToken(mozilla::dom::Element* aElement,
                                          nsAtom* aAttr);

  /**
   * Return document accessible for the given DOM node.
   */
  static DocAccessible* GetDocAccessibleFor(nsINode* aNode) {
    return GetAccService()->GetDocAccessible(
        nsCoreUtils::GetPresShellFor(aNode));
  }

  /**
   * Return document accessible for the given docshell.
   */
  static DocAccessible* GetDocAccessibleFor(nsIDocShellTreeItem* aContainer) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
    return GetAccService()->GetDocAccessible(docShell->GetPresShell());
  }

  /**
   * Return single or multi selectable container for the given item.
   *
   * @param  aAccessible  [in] the item accessible
   * @param  aState       [in] the state of the item accessible
   */
  static LocalAccessible* GetSelectableContainer(LocalAccessible* aAccessible,
                                                 uint64_t aState);

  /**
   * Return a text container accessible for the given node.
   */
  static HyperTextAccessible* GetTextContainer(nsINode* aNode);

  static Accessible* TableFor(Accessible* aRow);
  static LocalAccessible* TableFor(LocalAccessible* aRow);

  /**
   * Return true if the DOM node of a given accessible has a given attribute
   * with a value of "true".
   */
  static bool IsDOMAttrTrue(const LocalAccessible* aAccessible, nsAtom* aAttr);

  /**
   * Return true if the DOM node of given accessible has aria-selected="true"
   * attribute.
   */
  static inline bool IsARIASelected(const LocalAccessible* aAccessible) {
    return IsDOMAttrTrue(aAccessible, nsGkAtoms::aria_selected);
  }

  /**
   * Return true if the DOM node of given accessible has
   * aria-multiselectable="true" attribute.
   */
  static inline bool IsARIAMultiSelectable(const LocalAccessible* aAccessible) {
    return IsDOMAttrTrue(aAccessible, nsGkAtoms::aria_multiselectable);
  }

  /**
   * Converts the given coordinates to coordinates relative screen.
   *
   * @param aX               [in] the given x coord in dev pixels
   * @param aY               [in] the given y coord in dev pixels
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessible      [in] the accessible if coordinates are given
   *                         relative it.
   * @return converted coordinates
   */
  static LayoutDeviceIntPoint ConvertToScreenCoords(int32_t aX, int32_t aY,
                                                    uint32_t aCoordinateType,
                                                    Accessible* aAccessible);

  /**
   * Converts the given coordinates relative screen to another coordinate
   * system.
   *
   * @param aX               [in, out] the given x coord in dev pixels
   * @param aY               [in, out] the given y coord in dev pixels
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessible      [in] the accessible if coordinates are given
   *                         relative it
   */
  static void ConvertScreenCoordsTo(int32_t* aX, int32_t* aY,
                                    uint32_t aCoordinateType,
                                    Accessible* aAccessible);

  /**
   * Returns screen-relative coordinates (in dev pixels) for the parent of the
   * given accessible.
   *
   * @param [in] aAccessible  the accessible
   */
  static LayoutDeviceIntPoint GetScreenCoordsForParent(Accessible* aAccessible);

  /**
   * Returns coordinates in device pixels relative screen for the top level
   * window.
   *
   * @param aAccessible the acc hosted in the window.
   */
  static mozilla::LayoutDeviceIntPoint GetScreenCoordsForWindow(
      mozilla::a11y::Accessible* aAccessible);

  /**
   * Get the 'live' or 'container-live' object attribute value from the given
   * ELiveAttrRule constant.
   *
   * @param  aRule   [in] rule constant (see ELiveAttrRule in nsAccMap.h)
   * @param  aValue  [out] object attribute value
   *
   * @return         true if object attribute should be exposed
   */
  static bool GetLiveAttrValue(uint32_t aRule, nsAString& aValue);

#ifdef DEBUG
  /**
   * Detect whether the given accessible object implements nsIAccessibleText,
   * when it is text or has text child node.
   */
  static bool IsTextInterfaceSupportCorrect(LocalAccessible* aAccessible);
#endif

  /**
   * Return text length of the given accessible, return 0 on failure.
   */
  static uint32_t TextLength(Accessible* aAccessible);

  /**
   * Transform nsIAccessibleStates constants to internal state constant.
   */
  static inline uint64_t To64State(uint32_t aState1, uint32_t aState2) {
    return static_cast<uint64_t>(aState1) +
           (static_cast<uint64_t>(aState2) << 31);
  }

  /**
   * Transform internal state constant to nsIAccessibleStates constants.
   */
  static inline void To32States(uint64_t aState64, uint32_t* aState1,
                                uint32_t* aState2) {
    *aState1 = aState64 & 0x7fffffff;
    if (aState2) *aState2 = static_cast<uint32_t>(aState64 >> 31);
  }

  static uint32_t To32States(uint64_t aState, bool* aIsExtra) {
    uint32_t extraState = aState >> 31;
    *aIsExtra = !!extraState;
    return extraState ? extraState : aState;
  }

  /**
   * Return true if the given accessible can't have children. Used when exposing
   * to platform accessibility APIs, should the children be pruned off?
   */
  static bool MustPrune(Accessible* aAccessible);

  /**
   * Return true if the given accessible is within an ARIA live region; i.e.
   * the container-live attribute would be something other than "off" or empty.
   */
  static bool IsARIALive(const LocalAccessible* aAccessible);

  /**
   * Get the document Accessible which owns a given Accessible.
   * This function is needed because there is no unified base class for local
   * and remote documents.
   * If aAcc is null, null will be returned.
   */
  static Accessible* DocumentFor(Accessible* aAcc);

  /**
   * Get an Accessible in a given document by its unique id.
   * An Accessible's id can be obtained using Accessible::ID.
   * This function is needed because there is no unified base class for local
   * and remote documents.
   * If aDoc is nul, null will be returned.
   */
  static Accessible* GetAccessibleByID(Accessible* aDoc, uint64_t aID);

  /**
   * Get the URL for a given document.
   * This function is needed because there is no unified base class for local
   * and remote documents.
   */
  static void DocumentURL(Accessible* aDoc, nsAString& aURL);
};

}  // namespace a11y
}  // namespace mozilla

#endif
