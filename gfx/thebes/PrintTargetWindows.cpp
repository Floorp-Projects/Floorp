/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetWindows.h"

#include "cairo-win32.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "nsCoord.h"
#include "nsString.h"

namespace mozilla {
namespace gfx {

PrintTargetWindows::PrintTargetWindows(cairo_surface_t* aCairoSurface,
                                       const IntSize& aSize,
                                       HDC aDC)
  : PrintTarget(aCairoSurface, aSize)
  , mDC(aDC)
{
  // TODO: At least add basic memory reporting.
  // 4 * mSize.width * mSize.height + sizeof(PrintTargetWindows) ?
}

/* static */ already_AddRefed<PrintTargetWindows>
PrintTargetWindows::CreateOrNull(HDC aDC)
{
  // Figure out the cairo surface size - Windows we need to use the printable
  // area of the page.  Note: we only scale the printing using the LOGPIXELSY,
  // so we use that when calculating the surface width as well as the height.
  int32_t heightDPI = ::GetDeviceCaps(aDC, LOGPIXELSY);
  float width =
    (::GetDeviceCaps(aDC, HORZRES) * POINTS_PER_INCH_FLOAT) / heightDPI;
  float height =
    (::GetDeviceCaps(aDC, VERTRES) * POINTS_PER_INCH_FLOAT) / heightDPI;
  IntSize size(width, height);

  if (!Factory::CheckSurfaceSize(size)) {
    return nullptr;
  }

  cairo_surface_t* surface = cairo_win32_printing_surface_create(aDC);

  if (cairo_surface_status(surface)) {
    return nullptr;
  }

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetWindows> target =
    new PrintTargetWindows(surface, size, aDC);

  return target.forget();
}

nsresult
PrintTargetWindows::BeginPrinting(const nsAString& aTitle,
                                  const nsAString& aPrintToFileName)
{
  const uint32_t DOC_TITLE_LENGTH = MAX_PATH - 1;

  DOCINFOW docinfo;

  nsString titleStr(aTitle);
  if (titleStr.Length() > DOC_TITLE_LENGTH) {
    titleStr.SetLength(DOC_TITLE_LENGTH - 3);
    titleStr.AppendLiteral("...");
  }

  nsString docName(aPrintToFileName);
  docinfo.cbSize = sizeof(docinfo);
  docinfo.lpszDocName = titleStr.Length() > 0 ? titleStr.get() : L"Mozilla Document";
  docinfo.lpszOutput = docName.Length() > 0 ? docName.get() : nullptr;
  docinfo.lpszDatatype = nullptr;
  docinfo.fwType = 0;

  ::StartDocW(mDC, &docinfo);

  return NS_OK;
}

nsresult
PrintTargetWindows::EndPrinting()
{
  int result = ::EndDoc(mDC);
  return (result <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
PrintTargetWindows::AbortPrinting()
{
  int result = ::AbortDoc(mDC);
  return (result <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
PrintTargetWindows::BeginPage()
{
  int result = ::StartPage(mDC);
  return (result <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
PrintTargetWindows::EndPage()
{
  cairo_surface_show_page(mCairoSurface);
  int result = ::EndPage(mDC);
  return (result <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

} // namespace gfx
} // namespace mozilla
