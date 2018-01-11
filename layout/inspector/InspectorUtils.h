/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_InspectorUtils_h
#define mozilla_dom_InspectorUtils_h

#include "mozilla/dom/BindingDeclarations.h"

class nsAtom;
class nsIDocument;
class nsStyleContext;

namespace mozilla {
class StyleSheet;
namespace css {
class Rule;
} // namespace css
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

/**
 * A collection of utility methods for use by devtools.
 */
class InspectorUtils
{
public:
  static void GetAllStyleSheets(GlobalObject& aGlobal,
                                nsIDocument& aDocument,
                                nsTArray<RefPtr<StyleSheet>>& aResult);
  static void GetCSSStyleRules(GlobalObject& aGlobal,
                               Element& aElement,
                               const nsAString& aPseudo,
                               nsTArray<RefPtr<css::Rule>>& aResult);

  /**
   * Get the line number of a rule.
   *
   * @param aRule The rule.
   * @return The rule's line number.  Line numbers are 1-based.
   */
  static uint32_t GetRuleLine(GlobalObject& aGlobal, css::Rule& aRule);

  /**
   * Get the column number of a rule.
   *
   * @param aRule The rule.
   * @return The rule's column number.  Column numbers are 1-based.
   */
  static uint32_t GetRuleColumn(GlobalObject& aGlobal, css::Rule& aRule);

  /**
   * Like getRuleLine, but if the rule is in a <style> element,
   * returns a line number relative to the start of the element.
   *
   * @param aRule the rule to examine
   * @return the line number of the rule, possibly relative to the
   *         <style> element
   */
  static uint32_t GetRelativeRuleLine(GlobalObject& aGlobal, css::Rule& aRule);

private:
  static already_AddRefed<nsStyleContext>
    GetCleanStyleContextForElement(Element* aElement, nsAtom* aPseudo);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InspectorUtils_h
