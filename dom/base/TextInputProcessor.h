/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textinputprocessor_h_
#define mozilla_dom_textinputprocessor_h_

#include "mozilla/TextEventDispatcherListener.h"
#include "nsITextInputProcessor.h"
#include "nsITextInputProcessorCallback.h"

namespace mozilla {

namespace widget{
class TextEventDispatcher;
} // namespace widget

class TextInputProcessor MOZ_FINAL : public nsITextInputProcessor
                                   , public widget::TextEventDispatcherListener
{
  typedef mozilla::widget::IMENotification IMENotification;
  typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;

public:
  TextInputProcessor();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITEXTINPUTPROCESSOR

  // TextEventDispatcherListener
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) MOZ_OVERRIDE;
  NS_IMETHOD_(void)
    OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) MOZ_OVERRIDE;

protected:
  virtual ~TextInputProcessor();

private:
  nsresult InitInternal(nsIDOMWindow* aWindow,
                        nsITextInputProcessorCallback* aCallback,
                        bool aForTests,
                        bool& aSucceeded);
  nsresult CommitCompositionInternal(const nsAString* aCommitString = nullptr,
                                     bool* aSucceeded = nullptr);
  nsresult CancelCompositionInternal();
  nsresult IsValidStateForComposition();
  void UnlinkFromTextEventDispatcher();

  TextEventDispatcher* mDispatcher; // [Weak]
  nsCOMPtr<nsITextInputProcessorCallback> mCallback;
  bool mForTests;
};

} // namespace mozilla

#endif // #ifndef mozilla_dom_textinputprocessor_h_
