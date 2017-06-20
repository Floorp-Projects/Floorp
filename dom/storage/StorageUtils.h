/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageUtils_h
#define mozilla_dom_StorageUtils_h

class nsIPrincipal;

namespace mozilla {
namespace dom {
namespace StorageUtils {

nsresult
GenerateOriginKey(nsIPrincipal* aPrincipal, nsACString& aOriginAttrSuffix,
                  nsACString& aOriginKey);

bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal,
                nsIPrincipal* aSubjectPrincipal);

void
ReverseString(const nsACString& aSource, nsACString& aResult);

nsresult
CreateReversedDomain(const nsACString& aAsciiDomain,
                     nsACString& aKey);

} // StorageUtils namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_StorageUtils_h
