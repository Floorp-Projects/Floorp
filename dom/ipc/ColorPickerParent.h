/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=4 ts=8 et tw=80 :
 * This Source Code Form is subject to the terms of the Mozilla Public
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

  virtual bool RecvOpen() MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  class ColorPickerShownCallback MOZ_FINAL
    : public nsIColorPickerShownCallback
  {
  public:
    ColorPickerShownCallback(ColorPickerParent* aColorPickerParnet)
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

  nsRefPtr<ColorPickerShownCallback> mCallback;
  nsCOMPtr<nsIColorPicker> mPicker;

  nsString mTitle;
  nsString mInitialColor;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ColorPickerParent_h