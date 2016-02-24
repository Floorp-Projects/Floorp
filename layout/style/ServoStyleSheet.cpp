/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"

using namespace mozilla::dom;

namespace mozilla {

nsIURI*
ServoStyleSheet::GetSheetURI() const
{
  MOZ_CRASH("stylo: not implemented");
}

nsIURI*
ServoStyleSheet::GetOriginalURI() const
{
  MOZ_CRASH("stylo: not implemented");
}

nsIURI*
ServoStyleSheet::GetBaseURI() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI, nsIURI* aBaseURI)
{
  MOZ_CRASH("stylo: not implemented");
}

bool
ServoStyleSheet::IsApplicable() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetComplete()
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetParsingMode(css::SheetParsingMode aMode)
{
  MOZ_CRASH("stylo: not implemented");
}

bool
ServoStyleSheet::HasRules() const
{
  MOZ_CRASH("stylo: not implemented");
}

nsIDocument*
ServoStyleSheet::GetOwningDocument() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetOwningNode(nsINode* aOwningNode)
{
  MOZ_CRASH("stylo: not implemented");
}

nsINode*
ServoStyleSheet::GetOwnerNode() const
{
  MOZ_CRASH("stylo: not implemented");
}

StyleSheetHandle
ServoStyleSheet::GetParentSheet() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::AppendStyleSheet(StyleSheetHandle aSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

nsIPrincipal*
ServoStyleSheet::Principal() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::SetPrincipal(nsIPrincipal* aPrincipal)
{
  MOZ_CRASH("stylo: not implemented");
}

CORSMode
ServoStyleSheet::GetCORSMode() const
{
  MOZ_CRASH("stylo: not implemented");
}

net::ReferrerPolicy
ServoStyleSheet::GetReferrerPolicy() const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::GetIntegrity(dom::SRIMetadata& aResult) const
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::ParseSheet(const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber,
                            css::SheetParsingMode aParsingMode)
{
  MOZ_CRASH("stylo: not implemented");
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  MOZ_CRASH("stylo: not implemented");
}

#ifdef DEBUG
void
ServoStyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  MOZ_CRASH("stylo: not implemented");
}
#endif

} // namespace mozilla
