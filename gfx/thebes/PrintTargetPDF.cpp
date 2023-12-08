/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetPDF.h"

#include "cairo.h"
#include "cairo-pdf.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/StaticPrefs_print.h"
#include "nsContentUtils.h"
#include "nsString.h"

namespace mozilla::gfx {

static cairo_status_t write_func(void* closure, const unsigned char* data,
                                 unsigned int length) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return CAIRO_STATUS_SUCCESS;
  }
  nsCOMPtr<nsIOutputStream> out = reinterpret_cast<nsIOutputStream*>(closure);
  do {
    uint32_t wrote = 0;
    if (NS_FAILED(out->Write((const char*)data, length, &wrote))) {
      break;
    }
    data += wrote;
    length -= wrote;
  } while (length > 0);
  NS_ASSERTION(length == 0, "not everything was written to the file");
  return CAIRO_STATUS_SUCCESS;
}

PrintTargetPDF::PrintTargetPDF(cairo_surface_t* aCairoSurface,
                               const IntSize& aSize, nsIOutputStream* aStream)
    : PrintTarget(aCairoSurface, aSize), mStream(aStream) {}

PrintTargetPDF::~PrintTargetPDF() {
  // We get called first, then PrintTarget's dtor.  That means that mStream
  // is destroyed before PrintTarget's dtor calls cairo_surface_destroy.  This
  // can be a problem if Finish() hasn't been called on us, since
  // cairo_surface_destroy will then call cairo_surface_finish and that will
  // end up invoking write_func above with the by now dangling pointer mStream
  // that mCairoSurface stored.  To prevent that from happening we must call
  // Flush here before mStream is deleted.
  Finish();
}

/* static */
already_AddRefed<PrintTargetPDF> PrintTargetPDF::CreateOrNull(
    nsIOutputStream* aStream, const IntSize& aSizeInPoints) {
  if (NS_WARN_IF(!aStream)) {
    return nullptr;
  }

  cairo_surface_t* surface = cairo_pdf_surface_create_for_stream(
      write_func, (void*)aStream, aSizeInPoints.width, aSizeInPoints.height);
  if (cairo_surface_status(surface)) {
    return nullptr;
  }

  nsAutoString creatorName;
  if (NS_SUCCEEDED(nsContentUtils::GetLocalizedString(
          nsContentUtils::eBRAND_PROPERTIES, "brandFullName", creatorName)) &&
      !creatorName.IsEmpty()) {
    creatorName.Append(u" " MOZILLA_VERSION);
    cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_CREATOR,
                                   NS_ConvertUTF16toUTF8(creatorName).get());
  }

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetPDF> target =
      new PrintTargetPDF(surface, aSizeInPoints, aStream);
  return target.forget();
}

nsresult PrintTargetPDF::BeginPage(const IntSize& aSizeInPoints) {
  if (StaticPrefs::
          print_save_as_pdf_use_page_rule_size_as_paper_size_enabled()) {
    cairo_pdf_surface_set_size(mCairoSurface, aSizeInPoints.width,
                               aSizeInPoints.height);
    if (cairo_surface_status(mCairoSurface)) {
      return NS_ERROR_FAILURE;
    }
  }
  return PrintTarget::BeginPage(aSizeInPoints);
}

nsresult PrintTargetPDF::EndPage() {
  cairo_surface_show_page(mCairoSurface);
  return PrintTarget::EndPage();
}

void PrintTargetPDF::Finish() {
  if (mIsFinished ||
      AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    // We don't want to call Close() on mStream more than once, and we don't
    // want to block shutdown if for some reason the user shuts down the
    // browser mid print.
    return;
  }
  PrintTarget::Finish();
  mStream->Close();
}

}  // namespace mozilla::gfx
