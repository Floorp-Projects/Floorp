/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_StreamUtils_h
#define mozilla_dom_cache_StreamUtils_h

#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace dom {
namespace cache {

class Feature;
class CacheRequest;
class CacheResponse;
class CacheResponseOrVoid;

void StartDestroyStreamChild(const CacheResponseOrVoid& aResponseOrVoid);
void StartDestroyStreamChild(const CacheResponse& aResponse);
void StartDestroyStreamChild(const nsTArray<CacheResponse>& aResponses);
void StartDestroyStreamChild(const nsTArray<CacheRequest>& aRequests);

void AddFeatureToStreamChild(const CacheResponseOrVoid& aResponseOrVoid,
                             Feature* aFeature);
void AddFeatureToStreamChild(const CacheResponse& aResponse,
                             Feature* aFeature);
void AddFeatureToStreamChild(const nsTArray<CacheResponse>& aResponses,
                              Feature* aFeature);
void AddFeatureToStreamChild(const nsTArray<CacheRequest>& aRequests,
                              Feature* aFeature);

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_StreamUtils_h
