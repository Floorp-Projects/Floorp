/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCK_H_
#define DOM_QUOTA_DIRECTORYLOCK_H_

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/PersistenceType.h"

namespace mozilla::dom::quota {

struct GroupAndOrigin;
class OpenDirectoryListener;

class NS_NO_VTABLE RefCountedObject {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
};

class DirectoryLock : public RefCountedObject {
  friend class DirectoryLockImpl;

 public:
  int64_t Id() const;

  // 'Get' prefix is to avoid name collisions with the enum
  PersistenceType GetPersistenceType() const;

  quota::GroupAndOrigin GroupAndOrigin() const;

  const nsACString& Origin() const;

  Client::Type ClientType() const;

  void Acquire(RefPtr<OpenDirectoryListener> aOpenListener);

  RefPtr<DirectoryLock> Specialize(PersistenceType aPersistenceType,
                                   const quota::GroupAndOrigin& aGroupAndOrigin,
                                   Client::Type aClientType) const;

  void Log() const;

 private:
  DirectoryLock() = default;

  ~DirectoryLock() = default;
};

class NS_NO_VTABLE OpenDirectoryListener : public RefCountedObject {
 public:
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) = 0;

  virtual void DirectoryLockFailed() = 0;

 protected:
  virtual ~OpenDirectoryListener() = default;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCK_H_
