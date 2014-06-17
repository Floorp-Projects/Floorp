/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MemoryStreams_h
#define mozilla_dom_MemoryStreams_h

#include "nsIOutputStream.h"
#include "nsString.h"

template <class> struct already_AddRefed;

namespace mozilla {
namespace dom {

class MemoryOutputStream : public nsIOutputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  static already_AddRefed<MemoryOutputStream>
  Create(uint64_t aSize);

  const nsCString&
  Data() const
  {
    return mData;
  }

private:
  MemoryOutputStream()
  : mOffset(0)
  { }

  virtual ~MemoryOutputStream()
  { }

  nsCString mData;
  uint64_t mOffset;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MemoryStreams_h
