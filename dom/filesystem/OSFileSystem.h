/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_OSFileSystem_h
#define mozilla_dom_OSFileSystem_h

#include "mozilla/dom/FileSystemBase.h"

namespace mozilla::dom {

class OSFileSystem final : public FileSystemBase {
 public:
  explicit OSFileSystem(const nsAString& aRootDir);

  void Init(nsIGlobalObject* aGlobal);

  // Overrides FileSystemBase

  virtual already_AddRefed<FileSystemBase> Clone() override;

  virtual bool ShouldCreateDirectory() override {
    MOZ_CRASH("This should not be called.");
    // Because OSFileSystem should not be used when the creation of directories
    // is needed. For that we have OSFileSystemParent.
    return false;
  }

  virtual nsIGlobalObject* GetParentObject() const override;

  virtual bool IsSafeFile(nsIFile* aFile) const override;

  virtual bool IsSafeDirectory(Directory* aDir) const override;

  virtual void SerializeDOMPath(nsAString& aOutput) const override;

  // CC methods
  virtual void Unlink() override;
  virtual void Traverse(nsCycleCollectionTraversalCallback& cb) override;

 private:
  virtual ~OSFileSystem() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

class OSFileSystemParent final : public FileSystemBase {
 public:
  explicit OSFileSystemParent(const nsAString& aRootDir);

  // Overrides FileSystemBase

  virtual already_AddRefed<FileSystemBase> Clone() override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
    return nullptr;
  }

  virtual bool ShouldCreateDirectory() override { return false; }

  virtual nsIGlobalObject* GetParentObject() const override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
    return nullptr;
  }

  virtual void GetDirectoryName(nsIFile* aFile, nsAString& aRetval,
                                ErrorResult& aRv) const override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
  }

  virtual bool IsSafeFile(nsIFile* aFile) const override { return true; }

  virtual bool IsSafeDirectory(Directory* aDir) const override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
    return true;
  }

  virtual void SerializeDOMPath(nsAString& aOutput) const override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
  }

  // CC methods
  virtual void Unlink() override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
  }

  virtual void Traverse(nsCycleCollectionTraversalCallback& cb) override {
    MOZ_CRASH("This should not be called on the PBackground thread.");
  }

 private:
  virtual ~OSFileSystemParent() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_OSFileSystem_h
