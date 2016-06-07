/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebPublishOptionsIPCSerialiser_h
#define mozilla_dom_FlyWebPublishOptionsIPCSerialiser_h

#include "mozilla/dom/FlyWebPublishBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::FlyWebPublishOptions>
{
  typedef mozilla::dom::FlyWebPublishOptions paramType;

  // Function to serialize a FlyWebPublishOptions
  static void Write(Message *aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mUiUrl);
  }
  // Function to de-serialize a FlyWebPublishOptions
  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &(aResult->mUiUrl));
  }
};

}

#endif // mozilla_dom_FlyWebPublishOptionsIPCSerialiser_h
