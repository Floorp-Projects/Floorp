/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETWINDOWS_H
#define MOZILLA_GFX_PRINTTARGETWINDOWS_H

#include "PrintTarget.h"

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>

namespace mozilla {
namespace gfx {

/**
 * Windows printing target.
 */
class PrintTargetWindows final : public PrintTarget
{
public:
  static already_AddRefed<PrintTargetWindows>
  CreateOrNull(HDC aDC);

  virtual nsresult BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName,
                                 int32_t aStartPage,
                                 int32_t aEndPage) override;
  virtual nsresult EndPrinting() override;
  virtual nsresult AbortPrinting() override;
  virtual nsresult BeginPage() override;
  virtual nsresult EndPage() override;

private:
  PrintTargetWindows(cairo_surface_t* aCairoSurface,
                     const IntSize& aSize,
                     HDC aDC);
  HDC mDC;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETWINDOWS_H */
