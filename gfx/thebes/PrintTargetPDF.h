/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETPDF_H
#define MOZILLA_GFX_PRINTTARGETPDF_H

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "PrintTarget.h"

namespace mozilla {
namespace gfx {

/**
 * PDF printing target.
 */
class PrintTargetPDF final : public PrintTarget {
 public:
  static already_AddRefed<PrintTargetPDF> CreateOrNull(
      nsIOutputStream* aStream, const IntSize& aSizeInPoints);

  nsresult EndPage() override;
  void Finish() override;

 private:
  PrintTargetPDF(cairo_surface_t* aCairoSurface, const IntSize& aSize,
                 nsIOutputStream* aStream);
  virtual ~PrintTargetPDF();

  nsCOMPtr<nsIOutputStream> mStream;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETPDF_H */
