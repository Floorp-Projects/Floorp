/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHandleCommon_h
#define mozilla_dom_FileHandleCommon_h

#include "nscore.h"

#ifdef DEBUG
#define DEBUGONLY(...)  __VA_ARGS__
#else
#define DEBUGONLY(...)  /* nothing */
#endif

struct PRThread;

namespace mozilla {
namespace dom {

class RefCountedObject
{
public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;

protected:
  virtual ~RefCountedObject()
  { }
};

class ThreadObject
{
  DEBUGONLY(PRThread* mOwningThread;)

public:
  explicit ThreadObject(DEBUGONLY(PRThread* aOwningThread))
    DEBUGONLY(: mOwningThread(aOwningThread))
  { }

  virtual ~ThreadObject()
  { }

#ifdef DEBUG
  void
  AssertIsOnOwningThread() const;

  PRThread*
  OwningThread() const;
#else
  void
  AssertIsOnOwningThread() const
  { }
#endif
};

class RefCountedThreadObject
  : public RefCountedObject
  , public ThreadObject
{
public:
  explicit RefCountedThreadObject(DEBUGONLY(PRThread* aOwningThread))
    : ThreadObject(DEBUGONLY(aOwningThread))
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHandleCommon_h
