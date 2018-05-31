/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing data from presentational
 * HTML attributes
 */

#ifndef nsHTMLStyleSheet_h_
#define nsHTMLStyleSheet_h_

#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"

class nsIDocument;
class nsMappedAttributes;
struct RawServoDeclarationBlock;

class nsHTMLStyleSheet final
{
public:
  explicit nsHTMLStyleSheet(nsIDocument* aDocument);

  void SetOwningDocument(nsIDocument* aDocument);

  NS_INLINE_DECL_REFCOUNTING(nsHTMLStyleSheet)

  size_t DOMSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  void Reset();
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  const RefPtr<RawServoDeclarationBlock>& GetServoUnvisitedLinkDecl() const {
    return mServoUnvisitedLinkDecl;
  }
  const RefPtr<RawServoDeclarationBlock>& GetServoVisitedLinkDecl() const {
    return mServoVisitedLinkDecl;
  }
  const RefPtr<RawServoDeclarationBlock>& GetServoActiveLinkDecl() const {
    return mServoActiveLinkDecl;
  }

  // Mapped Attribute management methods
  already_AddRefed<nsMappedAttributes>
    UniqueMappedAttributes(nsMappedAttributes* aMapped);
  void DropMappedAttributes(nsMappedAttributes* aMapped);
  // For each mapped presentation attribute in the cache, resolve
  // the attached DeclarationBlock by running the mapping
  // and converting the ruledata to Servo specified values.
  void CalculateMappedServoDeclarations();

private:
  nsHTMLStyleSheet(const nsHTMLStyleSheet& aCopy) = delete;
  nsHTMLStyleSheet& operator=(const nsHTMLStyleSheet& aCopy) = delete;

  ~nsHTMLStyleSheet() {}


  // Implementation of SetLink/VisitedLink/ActiveLinkColor
  nsresult ImplLinkColorSetter(
      RefPtr<RawServoDeclarationBlock>& aDecl,
      nscolor aColor);

public: // for mLangRuleTable structures only


private:
  nsIDocument*            mDocument;
  RefPtr<RawServoDeclarationBlock> mServoUnvisitedLinkDecl;
  RefPtr<RawServoDeclarationBlock> mServoVisitedLinkDecl;
  RefPtr<RawServoDeclarationBlock> mServoActiveLinkDecl;

  PLDHashTable            mMappedAttrTable;
  // Whether or not the mapped attributes table
  // has been changed since the last call to
  // CalculateMappedServoDeclarations()
  bool                    mMappedAttrsDirty;
};

#endif /* !defined(nsHTMLStyleSheet_h_) */
