/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTINGTARGETPS_H
#define MOZILLA_GFX_PRINTINGTARGETPS_H

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "PrintTarget.h"

namespace mozilla {
namespace gfx {

/**
 * PostScript printing target.
 */
class PrintTargetPS final : public PrintTarget {
public:
  enum PageOrientation {
    PORTRAIT,
    LANDSCAPE
  };

  static already_AddRefed<PrintTargetPS>
  CreateOrNull(nsIOutputStream *aStream,
               IntSize aSizeInPoints,
               PageOrientation aOrientation);

  virtual nsresult BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName,
                                 int32_t aStartPage,
                                 int32_t aEndPage) override;
  virtual nsresult EndPage() override;
  virtual void Finish() override;

  virtual bool GetRotateForLandscape() {
    return (mOrientation == LANDSCAPE);
  }

private:
  PrintTargetPS(cairo_surface_t* aCairoSurface,
                const IntSize& aSize,
                nsIOutputStream *aStream,
                PageOrientation aOrientation);
  virtual ~PrintTargetPS();

  nsCOMPtr<nsIOutputStream> mStream;
  PageOrientation mOrientation;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTINGTARGETPS_H */
