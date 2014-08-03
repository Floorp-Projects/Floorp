/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataStoreCallbacks_h
#define mozilla_dom_DataStoreCallbacks_h

#include "nsISupports.h"

namespace mozilla {
namespace dom {

class DataStoreDB;

class DataStoreDBCallback
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  enum RunStatus {
    Success,
    CreatedSchema,
    Error
  };

  virtual void Run(DataStoreDB* aDb, RunStatus aStatus) = 0;

protected:
  virtual ~DataStoreDBCallback()
  {
  }
};

class DataStoreRevisionCallback
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  virtual void Run(const nsAString& aRevisionID) = 0;

protected:
  virtual ~DataStoreRevisionCallback()
  {
  }
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_DataStoreCallbacks_h
