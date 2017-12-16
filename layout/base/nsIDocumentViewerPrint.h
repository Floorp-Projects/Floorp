/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentViewerPrint_h___
#define nsIDocumentViewerPrint_h___

#include "nsISupports.h"

class nsIDocument;
namespace mozilla {
class StyleSetHandle;
} // namespace mozilla
class nsIPresShell;
class nsPresContext;
class nsViewManager;

// {c6f255cf-cadd-4382-b57f-cd2a9874169b}
#define NS_IDOCUMENT_VIEWER_PRINT_IID \
{ 0xc6f255cf, 0xcadd, 0x4382, \
  { 0xb5, 0x7f, 0xcd, 0x2a, 0x98, 0x74, 0x16, 0x9b } }

/**
 * A DocumentViewerPrint is an INTERNAL Interface used for interaction
 * between the DocumentViewer and nsPrintJob.
 */
class nsIDocumentViewerPrint : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_VIEWER_PRINT_IID)

  virtual void SetIsPrinting(bool aIsPrinting) = 0;
  virtual bool GetIsPrinting() = 0;

  virtual void SetIsPrintPreview(bool aIsPrintPreview) = 0;
  virtual bool GetIsPrintPreview() = 0;

  // The style set returned by CreateStyleSet is in the middle of an
  // update batch so that the caller can add sheets to it if needed.
  // Callers should call EndUpdate() on it when ready to use.
  virtual mozilla::StyleSetHandle CreateStyleSet(nsIDocument* aDocument) = 0;

  virtual void IncrementDestroyRefCount() = 0;

  virtual void ReturnToGalleyPresentation() = 0;

  virtual void OnDonePrinting() = 0;

  /**
   * Returns true is InitializeForPrintPreview() has been called.
   */
  virtual bool IsInitializedForPrintPreview() = 0;

  /**
   * Marks this viewer to be used for print preview.
   */
  virtual void InitializeForPrintPreview() = 0;

  /**
   * Replaces the current presentation with print preview presentation.
   */
  virtual void SetPrintPreviewPresentation(nsViewManager* aViewManager,
                                           nsPresContext* aPresContext,
                                           nsIPresShell* aPresShell) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentViewerPrint,
                              NS_IDOCUMENT_VIEWER_PRINT_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIDOCUMENTVIEWERPRINT \
  void SetIsPrinting(bool aIsPrinting) override; \
  bool GetIsPrinting() override; \
  void SetIsPrintPreview(bool aIsPrintPreview) override; \
  bool GetIsPrintPreview() override; \
  mozilla::StyleSetHandle CreateStyleSet(nsIDocument* aDocument) override; \
  void IncrementDestroyRefCount() override; \
  void ReturnToGalleyPresentation() override; \
  void OnDonePrinting() override; \
  bool IsInitializedForPrintPreview() override; \
  void InitializeForPrintPreview() override; \
  void SetPrintPreviewPresentation(nsViewManager* aViewManager, \
                                   nsPresContext* aPresContext, \
                                   nsIPresShell* aPresShell) override;

#endif /* nsIDocumentViewerPrint_h___ */
