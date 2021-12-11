/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETCG_H
#define MOZILLA_GFX_PRINTTARGETCG_H

#include <Carbon/Carbon.h>
#include "PrintTarget.h"

namespace mozilla {
namespace gfx {

/**
 * CoreGraphics printing target.
 */
class PrintTargetCG final : public PrintTarget {
 public:
  static already_AddRefed<PrintTargetCG> CreateOrNull(
      PMPrintSession aPrintSession, PMPageFormat aPageFormat,
      PMPrintSettings aPrintSettings, const IntSize& aSize);

  nsresult BeginPrinting(const nsAString& aTitle,
                         const nsAString& aPrintToFileName, int32_t aStartPage,
                         int32_t aEndPage) final;
  nsresult EndPrinting() final;
  nsresult AbortPrinting() final;
  nsresult BeginPage() final;
  nsresult EndPage() final;

  already_AddRefed<DrawTarget> GetReferenceDrawTarget() final;

 private:
  PrintTargetCG(PMPrintSession aPrintSession, PMPageFormat aPageFormat,
                PMPrintSettings aPrintSettings, const IntSize& aSize);
  ~PrintTargetCG();

  PMPrintSession mPrintSession;
  PMPageFormat mPageFormat;
  PMPrintSettings mPrintSettings;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETCG_H */
