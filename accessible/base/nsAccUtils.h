/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAccUtils_h_
#define nsAccUtils_h_

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/DocManager.h"

#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"

#include "nsIDocShell.h"
#include "nsPoint.h"

namespace mozilla {

namespace dom {
class Element;
}

namespace a11y {

class HyperTextAccessible;
class DocAccessible;

class nsAccUtils
{
public:
  /**
   * Returns value of attribute from the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - value of attribute
   */
  static void GetAccAttr(nsIPersistentProperties *aAttributes,
                         nsAtom *aAttrName,
                         nsAString& aAttrValue);

  /**
   * Set value of attribute for the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - new value of attribute
   */
  static void SetAccAttr(nsIPersistentProperties *aAttributes,
                         nsAtom *aAttrName,
                         const nsAString& aAttrValue);

  static void SetAccAttr(nsIPersistentProperties *aAttributes,
                         nsAtom* aAttrName,
                         nsAtom* aAttrValue);

  /**
   * Set group attributes ('level', 'setsize', 'posinset').
   */
  static void SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                               int32_t aLevel, int32_t aSetSize,
                               int32_t aPosInSet);

  /**
   * Get default value of the level for the given accessible.
   */
  static int32_t GetDefaultLevel(const Accessible* aAcc);

  /**
   * Return ARIA level value or the default one if ARIA is missed for the
   * given accessible.
   */
  static int32_t GetARIAOrDefaultLevel(const Accessible* aAccessible);

  /**
   * Compute group level for nsIDOMXULContainerItemElement node.
   */
  static int32_t GetLevelForXULContainerItem(nsIContent *aContent);

  /**
   * Set container-foo live region attributes for the given node.
   *
   * @param aAttributes    where to store the attributes
   * @param aStartContent  node to start from
   * @param aTopContent    node to end at
   */
  static void SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                         nsIContent* aStartContent,
                                         mozilla::dom::Element* aTopEl);

  /**
   * Any ARIA property of type boolean or NMTOKEN is undefined if the ARIA
   * property is not present, or is "" or "undefined". Do not call
   * this method for properties of type string, decimal, IDREF or IDREFS.
   *
   * Return true if the ARIA property is defined, otherwise false
   */
  static bool HasDefinedARIAToken(nsIContent *aContent, nsAtom *aAtom);

  /**
   * Return atomic value of ARIA attribute of boolean or NMTOKEN type.
   */
  static nsAtom* GetARIAToken(mozilla::dom::Element* aElement, nsAtom* aAttr);

  /**
   * Return document accessible for the given DOM node.
   */
  static DocAccessible* GetDocAccessibleFor(nsINode* aNode)
  {
    nsIPresShell *presShell = nsCoreUtils::GetPresShellFor(aNode);
    return GetAccService()->GetDocAccessible(presShell);
  }

  /**
   * Return document accessible for the given docshell.
   */
  static DocAccessible* GetDocAccessibleFor(nsIDocShellTreeItem* aContainer)
  {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
    return GetAccService()->GetDocAccessible(docShell->GetPresShell());
  }

  /**
   * Return single or multi selectable container for the given item.
   *
   * @param  aAccessible  [in] the item accessible
   * @param  aState       [in] the state of the item accessible
   */
  static Accessible* GetSelectableContainer(Accessible* aAccessible,
                                            uint64_t aState);

  /**
   * Return a text container accessible for the given node.
   */
  static HyperTextAccessible* GetTextContainer(nsINode* aNode);

  static Accessible* TableFor(Accessible* aRow);

  /**
   * Return true if the DOM node of given accessible has aria-selected="true"
   * attribute.
   */
  static bool IsARIASelected(Accessible* aAccessible);

  /**
   * Converts the given coordinates to coordinates relative screen.
   *
   * @param aX               [in] the given x coord
   * @param aY               [in] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessible      [in] the accessible if coordinates are given
   *                         relative it.
   * @return converted coordinates
   */
  static nsIntPoint ConvertToScreenCoords(int32_t aX, int32_t aY,
                                          uint32_t aCoordinateType,
                                          Accessible* aAccessible);

  /**
   * Converts the given coordinates relative screen to another coordinate
   * system.
   *
   * @param aX               [in, out] the given x coord
   * @param aY               [in, out] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessible      [in] the accessible if coordinates are given
   *                         relative it
   */
  static void ConvertScreenCoordsTo(int32_t* aX, int32_t* aY,
                                    uint32_t aCoordinateType,
                                    Accessible* aAccessible);

  /**
   * Returns coordinates relative screen for the parent of the given accessible.
   *
   * @param [in] aAccessible  the accessible
   */
  static nsIntPoint GetScreenCoordsForParent(Accessible* aAccessible);

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
  static bool IsTextInterfaceSupportCorrect(Accessible* aAccessible);
#endif

  /**
   * Return text length of the given accessible, return 0 on failure.
   */
  static uint32_t TextLength(Accessible* aAccessible);

  /**
   * Transform nsIAccessibleStates constants to internal state constant.
   */
  static inline uint64_t To64State(uint32_t aState1, uint32_t aState2)
  {
    return static_cast<uint64_t>(aState1) +
        (static_cast<uint64_t>(aState2) << 31);
  }

  /**
   * Transform internal state constant to nsIAccessibleStates constants.
   */
  static inline void To32States(uint64_t aState64,
                                uint32_t* aState1, uint32_t* aState2)
  {
    *aState1 = aState64 & 0x7fffffff;
    if (aState2)
      *aState2 = static_cast<uint32_t>(aState64 >> 31);
  }

  static uint32_t To32States(uint64_t aState, bool* aIsExtra)
  {
    uint32_t extraState = aState >> 31;
    *aIsExtra = !!extraState;
    return aState | extraState;
  }

  /**
   * Return true if the given accessible can't have children. Used when exposing
   * to platform accessibility APIs, should the children be pruned off?
   */
  static bool MustPrune(Accessible* aAccessible);
};

} // namespace a11y
} // namespace mozilla

#endif
