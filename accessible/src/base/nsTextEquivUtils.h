/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsTextEquivUtils_H_
#define _nsTextEquivUtils_H_

#include "Accessible.h"
#include "nsIStringBundle.h"
#include "Role.h"

class nsIContent;

/**
 * Text equivalent computation rules (see nsTextEquivUtils::gRoleToNameRulesMap)
 */
enum ETextEquivRule
{
  // No rule.
  eNoNameRule = 0x00,

  // Walk into subtree only if the currently navigated accessible is not root
  // accessible (i.e. if the accessible is part of text equivalent computation).
  eNameFromSubtreeIfReqRule = 0x01,

  // Text equivalent computation from subtree is allowed.
  eNameFromSubtreeRule = 0x03,

  // The accessible allows to append its value to text equivalent.
  // XXX: This is temporary solution. Once we move accessible value of links
  // and linkable accessibles to MSAA part we can remove this.
  eNameFromValueRule = 0x04
};

/**
 * The class provides utils methods to compute the accessible name and
 * description.
 */
class nsTextEquivUtils
{
public:
  typedef mozilla::a11y::Accessible Accessible;

  /**
   * Calculates the name from accessible subtree if allowed.
   *
   * @param aAccessible [in] the given accessible
   * @param aName       [out] accessible name
   */
  static nsresult GetNameFromSubtree(Accessible* aAccessible,
                                     nsAString& aName);

  /**
   * Calculates text equivalent from the subtree. Similar to GetNameFromSubtree.
   * The difference it returns not empty result for things like HTML p, i.e.
   * if the role has eNameFromSubtreeIfReq rule.
   */
  static void GetTextEquivFromSubtree(Accessible* aAccessible,
                                      nsString& aTextEquiv);

  /**
   * Calculates text equivalent for the given accessible from its IDRefs
   * attribute (like aria-labelledby or aria-describedby).
   *
   * @param aAccessible  [in] the accessible text equivalent is computed for
   * @param aIDRefsAttr  [in] IDRefs attribute on DOM node of the accessible
   * @param aTextEquiv   [out] result text equivalent
   */
  static nsresult GetTextEquivFromIDRefs(Accessible* aAccessible,
                                         nsIAtom *aIDRefsAttr,
                                         nsAString& aTextEquiv);

  /**
   * Calculates the text equivalent from the given content and its subtree if
   * allowed and appends it to the given string.
   *
   * @param aInitiatorAcc  [in] the accessible text equivalent is computed for
   *                       in the end (root accessible of text equivalent
   *                       calculation recursion)
   * @param aContent       [in] the given content the text equivalent is
   *                       computed from
   * @param aString        [in, out] the string
   */
  static nsresult AppendTextEquivFromContent(Accessible* aInitiatorAcc,
                                             nsIContent *aContent,
                                             nsAString *aString);

  /**
   * Calculates the text equivalent from the given text content (may be text
   * node or html:br) and appends it to the given string.
   *
   * @param aContent       [in] the text content
   * @param aString        [in, out] the string
   */
  static nsresult AppendTextEquivFromTextContent(nsIContent *aContent,
                                                 nsAString *aString);

private:
  /**
   * Iterates accessible children and calculates text equivalent from each
   * child.
   */
  static nsresult AppendFromAccessibleChildren(Accessible* aAccessible,
                                               nsAString *aString);
  
  /**
   * Calculates text equivalent from the given accessible and its subtree if
   * allowed.
   */
  static nsresult AppendFromAccessible(Accessible* aAccessible,
                                       nsAString *aString);

  /**
   * Calculates text equivalent from the value of given accessible.
   */
  static nsresult AppendFromValue(Accessible* aAccessible,
                                  nsAString *aString);
  /**
   * Iterates DOM children and calculates text equivalent from each child node.
   */
  static nsresult AppendFromDOMChildren(nsIContent *aContent,
                                        nsAString *aString);

  /**
   * Calculates text equivalent from the given DOM node and its subtree if
   * allowed.
   */
  static nsresult AppendFromDOMNode(nsIContent *aContent, nsAString *aString);

  /**
   * Concatenates strings and appends space between them. Returns true if
   * text equivalent string was appended.
   */
  static bool AppendString(nsAString *aString,
                             const nsAString& aTextEquivalent);

  /**
   * Returns the rule (constant of ETextEquivRule) for a given role.
   */
  static uint32_t GetRoleRule(mozilla::a11y::roles::Role aRole);
};

#endif
