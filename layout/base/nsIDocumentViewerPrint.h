/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIDocumentViewerPrint_h___
#define nsIDocumentViewerPrint_h___

#include "nsISupports.h"

class nsPresContext;
class nsIPrintSettings;
class nsPrintObject;
class nsISelection;
class nsIPresShell;
class nsIDocument;
class nsStyleSet;
class nsIContent;
class nsIWebShell;

// {D0B7F354-D575-43fd-903D-5AA35A193EDA}
#define NS_IDOCUMENT_VIEWER_PRINT_IID \
 { 0xd0b7f354, 0xd575, 0x43fd, { 0x90, 0x3d, 0x5a, 0xa3, 0x5a, 0x19, 0x3e, 0xda } }

/**
 * A DocumentViewerPrint is an INTERNAL Interface mainly used for interaction
 * between the DocumentViewer and the PrintEngine, although other objects may 
 * use to find out if printing or print preview is currently underway
 */
class nsIDocumentViewerPrint : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_VIEWER_PRINT_IID)

  virtual void SetIsPrinting(PRBool aIsPrinting) = 0;
  virtual PRBool GetIsPrinting() = 0;

  virtual void SetIsPrintPreview(PRBool aIsPrintPreview) = 0;
  virtual PRBool GetIsPrintPreview() = 0;

  virtual PRBool GetIsCreatingPrintPreview() = 0;

  // The style set returned by CreateStyleSet is in the middle of an
  // update batch so that the caller can add sheets to it if needed.
  // Callers should call EndUpdate() on it when ready to use.
  virtual nsresult CreateStyleSet(nsIDocument* aDocument, nsStyleSet** aStyleSet) = 0;

  virtual nsresult GetDocumentSelection(nsISelection **aSelection,
                                        nsIPresShell * aPresShell = nsnull) = 0;

  virtual void IncrementDestroyRefCount() = 0;

  virtual void ReturnToGalleyPresentation() = 0;

  virtual void InstallNewPresentation() = 0;

  virtual void OnDonePrinting() = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIDOCUMENTVIEWERPRINT \
  virtual void     SetIsPrinting(PRBool aIsPrinting); \
  virtual PRBool   GetIsPrinting(); \
  virtual void     SetIsPrintPreview(PRBool aIsPrintPreview); \
  virtual PRBool   GetIsPrintPreview(); \
  virtual PRBool   GetIsCreatingPrintPreview(); \
  virtual nsresult CreateStyleSet(nsIDocument* aDocument, nsStyleSet** aStyleSet); \
  virtual nsresult GetDocumentSelection(nsISelection **aSelection, nsIPresShell * aPresShell = nsnull); \
  virtual void     IncrementDestroyRefCount(); \
  virtual void     ReturnToGalleyPresentation(); \
  virtual void     InstallNewPresentation(); \
  virtual void     OnDonePrinting();

#endif /* nsIDocumentViewerPrint_h___ */
