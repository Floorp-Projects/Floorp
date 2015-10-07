/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ColorPickerParent_h
#define mozilla_dom_ColorPickerParent_h

#include "mozilla/dom/PColorPickerParent.h"
#include "nsIColorPicker.h"

namespace mozilla {
namespace dom {

class ColorPickerParent : public PColorPickerParent
{
 public:
  ColorPickerParent(const nsString& aTitle,
                    const nsString& aInitialColor)
  : mTitle(aTitle)
  , mInitialColor(aInitialColor)
  {}

  virtual bool RecvOpen() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  class ColorPickerShownCallback final
    : public nsIColorPickerShownCallback
  {
  public:
    explicit ColorPickerShownCallback(ColorPickerParent* aColorPickerParnet)
      : mColorPickerParent(aColorPickerParnet)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSICOLORPICKERSHOWNCALLBACK

    void Destroy();

  private:
    ~ColorPickerShownCallback() {}
    ColorPickerParent* mColorPickerParent;
  };

 private:
  virtual ~ColorPickerParent() {}

  bool CreateColorPicker();

  RefPtr<ColorPickerShownCallback> mCallback;
  nsCOMPtr<nsIColorPicker> mPicker;

  nsString mTitle;
  nsString mInitialColor;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ColorPickerParent_h
