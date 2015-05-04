/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FallbackEncoding_h_
#define mozilla_dom_FallbackEncoding_h_

#include "nsString.h"

namespace mozilla {
namespace dom {

class FallbackEncoding
{
public:

  /**
   * Whether FromTopLevelDomain() should be used.
   */
  static bool sGuessFallbackFromTopLevelDomain;

  /**
   * Gets the locale-dependent fallback encoding for legacy HTML and plain
   * text content.
   *
   * @param aFallback the outparam for the fallback encoding
   */
  static void FromLocale(nsACString& aFallback);

  /**
   * Checks if it is appropriate to call FromTopLevelDomain() for a given TLD.
   *
   * @param aTLD the top-level domain (in Punycode)
   * @return true if OK to call FromTopLevelDomain()
   */
  static bool IsParticipatingTopLevelDomain(const nsACString& aTLD);

  /**
   * Gets a top-level domain-depedendent fallback encoding for legacy HTML
   * and plain text content
   *
   * @param aTLD the top-level domain (in Punycode)
   * @param aFallback the outparam for the fallback encoding
   */
  static void FromTopLevelDomain(const nsACString& aTLD, nsACString& aFallback);

  // public API ends here!

  /**
   * Allocate sInstance used by FromLocale().
   * To be called from nsLayoutStatics only.
   */
  static void Initialize();

  /**
   * Delete sInstance used by FromLocale().
   * To be called from nsLayoutStatics only.
   */
  static void Shutdown();

private:

  /**
   * The fallback cache.
   */
  static FallbackEncoding* sInstance;

  FallbackEncoding();
  ~FallbackEncoding();

  /**
   * Invalidates the cache.
   */
  void Invalidate()
  {
    mFallback.Truncate();
  }

  static void PrefChanged(const char*, void*);

  /**
   * Gets the fallback encoding label.
   * @param aFallback the fallback encoding
   */
  void Get(nsACString& aFallback);

  nsCString mFallback;
};

} // dom
} // mozilla

#endif // mozilla_dom_FallbackEncoding_h_

