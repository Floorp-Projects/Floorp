/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChildProcessChannelListener_h
#define mozilla_dom_ChildProcessChannelListener_h

#include <functional>
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIChildProcessChannelListener.h"
#include "nsIChildChannel.h"
#include "mozilla/Variant.h"

namespace mozilla {
namespace dom {

class ChildProcessChannelListener final
    : public nsIChildProcessChannelListener {
 public:
  typedef std::function<void(nsIChildChannel*)> Callback;

  ChildProcessChannelListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICHILDPROCESSCHANNELLISTENER

  void RegisterCallback(uint64_t aIdentifier, Callback&& aCallback);

  static already_AddRefed<ChildProcessChannelListener> GetSingleton();

 private:
  virtual ~ChildProcessChannelListener();

  nsDataHashtable<nsUint64HashKey, Callback> mCallbacks;
  nsDataHashtable<nsUint64HashKey, nsCOMPtr<nsIChildChannel>> mChannels;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_ChildProcessChannelListener_h)
