/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PRINCIPALCHANGEOBSERVER_H_
#define MOZILLA_PRINCIPALCHANGEOBSERVER_H_

namespace mozilla {
namespace dom {

/**
 * A PrincipalChangeObserver for any type, but originating from DOMMediaStream,
 * then expanded to MediaStreamTrack.
 *
 * Used to learn about dynamic changes to an object's principal.
 * Operations relating to these observers must be confined to the main thread.
 */
template<typename T>
class PrincipalChangeObserver
{
public:
  virtual void PrincipalChanged(T* aArg) = 0;
};

} // namespace dom
} // namespace mozilla

#endif /* MOZILLA_PRINCIPALCHANGEOBSERVER_H_ */
