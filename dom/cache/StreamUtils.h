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
class PCacheRequest;
class PCacheResponse;
class PCacheResponseOrVoid;

void StartDestroyStreamChild(const PCacheResponseOrVoid& aResponseOrVoid);
void StartDestroyStreamChild(const PCacheResponse& aResponse);
void StartDestroyStreamChild(const nsTArray<PCacheResponse>& aResponses);
void StartDestroyStreamChild(const nsTArray<PCacheRequest>& aRequests);

void AddFeatureToStreamChild(const PCacheResponseOrVoid& aResponseOrVoid,
                             Feature* aFeature);
void AddFeatureToStreamChild(const PCacheResponse& aResponse,
                             Feature* aFeature);
void AddFeatureToStreamChild(const nsTArray<PCacheResponse>& aResponses,
                              Feature* aFeature);
void AddFeatureToStreamChild(const nsTArray<PCacheRequest>& aRequests,
                              Feature* aFeature);

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_StreamUtils_h
