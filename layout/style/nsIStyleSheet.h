/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface representing a collection of style data attached to a
 * document, which may be or be combined into a style rule processor
 */

#ifndef nsIStyleSheet_h___
#define nsIStyleSheet_h___

#include "mozilla/MemoryReporting.h"
#include <stdio.h>
#include "nsISupports.h"

class nsString;
class nsIURI;
class nsIDocument;

// IID for the nsIStyleSheet interface
// 3eb34a60-04bd-41d9-9f60-882694e61c38
#define NS_ISTYLE_SHEET_IID     \
{ 0x3eb34a60, 0x04bd, 0x41d9,   \
 { 0x9f, 0x60, 0x88, 0x26, 0x94, 0xe6, 0x1c, 0x38 } }

/**
 * A style sheet is a thing associated with a document that has style
 * rules.  Those style rules can be reached in one of two ways, depending
 * on which level of the nsStyleSet it is in:
 *   1) It can be |QueryInterface|d to nsIStyleRuleProcessor
 *   2) It can be |QueryInterface|d to nsCSSStyleSheet, with which the
 *      |nsStyleSet| uses an |nsCSSRuleProcessor| to access the rules.
 */
class nsIStyleSheet : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLE_SHEET_IID)

  // basic style sheet data
  virtual nsIURI* GetSheetURI() const = 0;
  virtual nsIURI* GetBaseURI() const = 0;
  virtual void GetTitle(nsString& aTitle) const = 0;
  virtual void GetType(nsString& aType) const = 0;
  virtual bool HasRules() const = 0;

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   *
   */
  virtual bool IsApplicable() const = 0;

  /**
   * Set the stylesheet to be enabled.  This may or may not make it
   * applicable.  Note that this WILL inform the sheet's document of
   * its new applicable state if the state changes but WILL NOT call
   * BeginUpdate() or EndUpdate() on the document -- calling those is
   * the caller's responsibility.  This allows use of SetEnabled when
   * batched updates are desired.  If you want updates handled for
   * you, see nsIDOMStyleSheet::SetDisabled().
   */
  virtual void SetEnabled(bool aEnabled) = 0;

  /**
   * Whether the sheet is complete.
   */
  virtual bool IsComplete() const = 0;
  virtual void SetComplete() = 0;

  // style sheet owner info
  virtual nsIStyleSheet* GetParentSheet() const = 0;  // may be null
  virtual nsIDocument* GetOwningDocument() const = 0; // may be null
  virtual void SetOwningDocument(nsIDocument* aDocument) = 0;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const = 0;
#endif

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleSheet, NS_ISTYLE_SHEET_IID)

#endif /* nsIStyleSheet_h___ */
