/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetPS.h"

#include "cairo.h"
#include "cairo-ps.h"

namespace mozilla {
namespace gfx {

static cairo_status_t
write_func(void *closure, const unsigned char *data, unsigned int length)
{
  nsCOMPtr<nsIOutputStream> out = reinterpret_cast<nsIOutputStream*>(closure);
  do {
    uint32_t wrote = 0;
    if (NS_FAILED(out->Write((const char*)data, length, &wrote))) {
      break;
    }
    data += wrote; length -= wrote;
  } while (length > 0);
  NS_ASSERTION(length == 0, "not everything was written to the file");
  return CAIRO_STATUS_SUCCESS;
}

PrintTargetPS::PrintTargetPS(cairo_surface_t* aCairoSurface,
                             const IntSize& aSize,
                             nsIOutputStream *aStream,
                             PageOrientation aOrientation)
  : PrintTarget(aCairoSurface, aSize)
  , mStream(aStream)
  , mOrientation(aOrientation)
{
}

PrintTargetPS::~PrintTargetPS()
{
  // We get called first, then PrintTarget's dtor.  That means that mStream
  // is destroyed before PrintTarget's dtor calls cairo_surface_destroy.  This
  // can be a problem if Finish() hasn't been called on us, since
  // cairo_surface_destroy will then call cairo_surface_finish and that will
  // end up invoking write_func above with the by now dangling pointer mStream
  // that mCairoSurface stored.  To prevent that from happening we must call
  // Flush here before mStream is deleted.
  Finish();
}

/* static */ already_AddRefed<PrintTargetPS>
PrintTargetPS::CreateOrNull(nsIOutputStream *aStream,
                            IntSize aSizeInPoints,
                            PageOrientation aOrientation)
{
  // The PS output does not specify the page size so to print landscape we need
  // to rotate the drawing 90 degrees and print on portrait paper.  If printing
  // landscape, swap the width/height supplied to cairo to select a portrait
  // print area.  Our consumers are responsible for checking
  // RotateForLandscape() and applying a rotation transform if true.
  if (aOrientation == LANDSCAPE) {
    Swap(aSizeInPoints.width, aSizeInPoints.height);
  }

  cairo_surface_t* surface =
    cairo_ps_surface_create_for_stream(write_func, (void*)aStream,
                                       aSizeInPoints.width,
                                       aSizeInPoints.height);
  if (cairo_surface_status(surface)) {
    return nullptr;
  }
  cairo_ps_surface_restrict_to_level(surface, CAIRO_PS_LEVEL_2);

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetPS> target = new PrintTargetPS(surface, aSizeInPoints,
                                                   aStream, aOrientation);
  return target.forget();
}

nsresult
PrintTargetPS::BeginPrinting(const nsAString& aTitle,
                             const nsAString& aPrintToFileName)
{
  if (mOrientation == PORTRAIT) {
    cairo_ps_surface_dsc_comment(mCairoSurface, "%%Orientation: Portrait");
  } else {
    cairo_ps_surface_dsc_comment(mCairoSurface, "%%Orientation: Landscape");
  }
  return NS_OK;
}

nsresult
PrintTargetPS::EndPage()
{
  cairo_surface_show_page(mCairoSurface);
  return NS_OK;
}

void
PrintTargetPS::Finish()
{
  if (mIsFinished) {
    return; // We don't want to call Close() on mStream more than once
  }
  PrintTarget::Finish();
  mStream->Close();
}

} // namespace gfx
} // namespace mozilla
