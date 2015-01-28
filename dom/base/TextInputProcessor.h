/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textinputprocessor_h_
#define mozilla_dom_textinputprocessor_h_

#include "nsITextInputProcessor.h"

namespace mozilla {

namespace widget{
class TextEventDispatcher;
} // namespace widget

class TextInputProcessor MOZ_FINAL : public nsITextInputProcessor
{
  typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;

public:
  TextInputProcessor();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITEXTINPUTPROCESSOR

private:
  ~TextInputProcessor();

  nsresult InitInternal(nsIDOMWindow* aWindow,
                        bool aForTests,
                        bool& aSucceeded);
  nsresult IsValidStateForComposition() const;

  TextEventDispatcher* mDispatcher; // [Weak]
  bool mIsInitialized;
  bool mForTests;
};

} // namespace mozilla

#endif // #ifndef mozilla_dom_textinputprocessor_h_
