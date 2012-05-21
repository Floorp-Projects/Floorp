/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsTextEquivUtils_H_
#define _nsTextEquivUtils_H_

#include "nsAccessible.h"

#include "nsIContent.h"
#include "nsIStringBundle.h"

/**
 * Text equivalent computation rules (see nsTextEquivUtils::gRoleToNameRulesMap)
 */
enum ETextEquivRule
{
  // No rule.
  eNoRule = 0x00,

  // Walk into subtree only if the currently navigated accessible is not root
  // accessible (i.e. if the accessible is part of text equivalent computation).
  eFromSubtreeIfRec = 0x01,

  // Text equivalent computation from subtree is allowed.
  eFromSubtree = 0x03,

  // The accessible allows to append its value to text equivalent.
  // XXX: This is temporary solution. Once we move accessible value of links
  // and linkable accessibles to MSAA part we can remove this.
  eFromValue = 0x04
};

/**
 * The class provides utils methods to compute the accessible name and
 * description.
 */
class nsTextEquivUtils
{
public:

  /**
   * Calculates the name from accessible subtree if allowed.
   *
   * @param aAccessible [in] the given accessible
   * @param aName       [out] accessible name
   */
  static nsresult GetNameFromSubtree(nsAccessible *aAccessible,
                                     nsAString& aName);

  /**
   * Calculates text equivalent for the given accessible from its IDRefs
   * attribute (like aria-labelledby or aria-describedby).
   *
   * @param aAccessible  [in] the accessible text equivalent is computed for
   * @param aIDRefsAttr  [in] IDRefs attribute on DOM node of the accessible
   * @param aTextEquiv   [out] result text equivalent
   */
  static nsresult GetTextEquivFromIDRefs(nsAccessible *aAccessible,
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
  static nsresult AppendTextEquivFromContent(nsAccessible *aInitiatorAcc,
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
  static nsresult AppendFromAccessibleChildren(nsAccessible *aAccessible,
                                               nsAString *aString);
  
  /**
   * Calculates text equivalent from the given accessible and its subtree if
   * allowed.
   */
  static nsresult AppendFromAccessible(nsAccessible *aAccessible,
                                       nsAString *aString);

  /**
   * Calculates text equivalent from the value of given accessible.
   */
  static nsresult AppendFromValue(nsAccessible *aAccessible,
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
   * Returns true if the given string is empty or contains whitespace symbols
   * only. In contrast to nsWhitespaceTokenizer class it takes into account
   * non-breaking space (0xa0).
   */
  static bool IsWhitespaceString(const nsSubstring& aString);

  /**
   * Returns true if the given character is whitespace symbol.
   */
  static bool IsWhitespace(PRUnichar aChar);

  /**
   * Map array from roles to name rules (constants of ETextEquivRule).
   */
  static PRUint32 gRoleToNameRulesMap[];

  /**
   * The accessible for which we are computing a text equivalent. It is useful
   * for bailing out during recursive text computation, or for special cases
   * like step f. of the ARIA implementation guide.
   */
  static nsRefPtr<nsAccessible> gInitiatorAcc;
};

#endif
