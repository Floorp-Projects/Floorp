/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Quotes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/intl/Locale.h"
#include "nsTHashMap.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::intl;

namespace {
struct LangQuotesRec {
  const char* mLangs;
  Quotes mQuotes;
};

#include "cldr-quotes.inc"

static StaticAutoPtr<nsTHashMap<nsCStringHashKey, Quotes>> sQuotesForLang;
}  // anonymous namespace

namespace mozilla {
namespace intl {

const Quotes* QuotesForLang(const nsAtom* aLang) {
  MOZ_ASSERT(NS_IsMainThread());

  // On first use, initialize the hashtable from our CLDR-derived data array.
  if (!sQuotesForLang) {
    sQuotesForLang = new nsTHashMap<nsCStringHashKey, Quotes>(32);
    ClearOnShutdown(&sQuotesForLang);
    for (const auto& i : sLangQuotes) {
      const char* s = i.mLangs;
      size_t len;
      while ((len = strlen(s))) {
        sQuotesForLang->InsertOrUpdate(nsDependentCString(s, len), i.mQuotes);
        s += len + 1;
      }
    }
  }

  nsAtomCString langStr(aLang);
  const Quotes* entry = sQuotesForLang->Lookup(langStr).DataPtrOrNull();
  if (entry) {
    // Found an exact match for the requested lang.
    return entry;
  }

  // Try parsing lang as a Locale and canonicalizing the subtags, then see if
  // we can match it with region or script subtags, if present, or just the
  // primary language tag.
  Locale loc;
  auto result = LocaleParser::TryParse(langStr, loc);
  if (result.isErr()) {
    return nullptr;
  }
  if (loc.Canonicalize().isErr()) {
    return nullptr;
  }
  if (loc.Region().Present()) {
    nsAutoCString langAndRegion;
    langAndRegion.Append(loc.Language().Span());
    langAndRegion.Append('-');
    langAndRegion.Append(loc.Region().Span());
    if ((entry = sQuotesForLang->Lookup(langAndRegion).DataPtrOrNull())) {
      return entry;
    }
  }
  if (loc.Script().Present()) {
    nsAutoCString langAndScript;
    langAndScript.Append(loc.Language().Span());
    langAndScript.Append('-');
    langAndScript.Append(loc.Script().Span());
    if ((entry = sQuotesForLang->Lookup(langAndScript).DataPtrOrNull())) {
      return entry;
    }
  }
  Span<const char> langAsSpan = loc.Language().Span();
  nsAutoCString lang(langAsSpan.data(), langAsSpan.size());
  if ((entry = sQuotesForLang->Lookup(lang).DataPtrOrNull())) {
    return entry;
  }

  return nullptr;
}

}  // namespace intl
}  // namespace mozilla
