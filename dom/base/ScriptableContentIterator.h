/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_scriptablecontentiterator_h
#define mozilla_scriptablecontentiterator_h

#include "mozilla/Attributes.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIScriptableContentIterator.h"

namespace mozilla {

class ScriptableContentIterator final : public nsIScriptableContentIterator {
 public:
  ScriptableContentIterator();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ScriptableContentIterator)
  NS_DECL_NSISCRIPTABLECONTENTITERATOR

 protected:
  virtual ~ScriptableContentIterator() = default;
  void EnsureContentIterator();

  IteratorType mIteratorType;
  UniquePtr<SafeContentIteratorBase> mContentIterator;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_scriptablecontentiterator_h
