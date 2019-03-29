/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextEditUtils_h
#define TextEditUtils_h

#include "nscore.h"

class nsINode;

namespace mozilla {

class TextEditor;

class TextEditUtils final {
 public:
  // from TextEditRules:
  static bool IsBody(nsINode* aNode);
  static bool IsBreak(nsINode* aNode);
  static bool IsMozBR(nsINode* aNode);
  static bool HasMozAttr(nsINode* aNode);
};

/***************************************************************************
 * stack based helper class for detecting end of editor initialization, in
 * order to trigger "end of init" initialization of the edit rules.
 */
class AutoEditInitRulesTrigger final {
 private:
  RefPtr<TextEditor> mTextEditor;
  nsresult& mResult;

 public:
  AutoEditInitRulesTrigger(TextEditor* aTextEditor, nsresult& aResult);
  MOZ_CAN_RUN_SCRIPT
  ~AutoEditInitRulesTrigger();
};

}  // namespace mozilla

#endif  // #ifndef TextEditUtils_h
