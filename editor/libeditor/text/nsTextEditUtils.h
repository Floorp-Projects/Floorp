/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextEditUtils_h__
#define nsTextEditUtils_h__

#include "nsError.h"  // for nsresult

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

class nsIDOMNode;
class nsPlaintextEditor;

class nsTextEditUtils
{
public:
  // from nsTextEditRules:
  static bool IsBody(nsIDOMNode* aNode);
  static bool IsBreak(nsIDOMNode* aNode);
  static bool IsMozBR(nsIDOMNode* aNode);
  static bool IsMozBR(mozilla::dom::Element* aNode);
  static bool HasMozAttr(nsIDOMNode* aNode);
};

/***************************************************************************
 * stack based helper class for detecting end of editor initialization, in
 * order to trigger "end of init" initialization of the edit rules.
 */
class nsAutoEditInitRulesTrigger
{
private:
  nsPlaintextEditor* mEd;
  nsresult& mRes;
public:
  nsAutoEditInitRulesTrigger(nsPlaintextEditor* aEd, nsresult& aRes);
  ~nsAutoEditInitRulesTrigger();
};

#endif /* nsTextEditUtils_h__ */
