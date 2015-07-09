/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

#include "mozilla/BasePrincipal.h"

namespace mozilla {
namespace dom {

/* static */ void
ChromeUtils::OriginAttributesToSuffix(dom::GlobalObject& aGlobal,
                                      const dom::OriginAttributesDictionary& aAttrs,
                                      nsCString& aSuffix)

{
  OriginAttributes attrs(aAttrs);
  attrs.CreateSuffix(aSuffix);
}

/* static */ bool
ChromeUtils::OriginAttributesMatchPattern(dom::GlobalObject& aGlobal,
                                          const dom::OriginAttributesDictionary& aAttrs,
                                          const dom::OriginAttributesPatternDictionary& aPattern)
{
  OriginAttributes attrs(aAttrs);
  OriginAttributesPattern pattern(aPattern);
  return pattern.Matches(attrs);
}

} // namespace dom
} // namespace mozilla
