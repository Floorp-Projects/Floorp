/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Directory_h
#define mozilla_dom_Directory_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMFile.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

// Resolve the name collision of Microsoft's API name with macros defined in
// Windows header files. Undefine the macro of CreateDirectory to avoid
// Directory#CreateDirectory being replaced by Directory#CreateDirectoryW.
#ifdef CreateDirectory
#undef CreateDirectory
#endif

namespace mozilla {
namespace dom {

class FileSystemBase;
class Promise;
class StringOrFileOrDirectory;

class Directory MOZ_FINAL
  : public nsISupports
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Directory)

public:
  static already_AddRefed<Promise>
  GetRoot(FileSystemBase* aFileSystem);

  Directory(FileSystemBase* aFileSystem, const nsAString& aPath);
  ~Directory();

  // ========= Begin WebIDL bindings. ===========

  nsPIDOMWindow*
  GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void
  GetName(nsString& aRetval) const;

  already_AddRefed<Promise>
  CreateDirectory(const nsAString& aPath);

  already_AddRefed<Promise>
  Get(const nsAString& aPath);

  already_AddRefed<Promise>
  Remove(const StringOrFileOrDirectory& aPath);

  already_AddRefed<Promise>
  RemoveDeep(const StringOrFileOrDirectory& aPath);

  // =========== End WebIDL bindings.============

  FileSystemBase*
  GetFileSystem() const;
private:
  static bool
  IsValidRelativePath(const nsString& aPath);

  /*
   * Convert relative DOM path to the absolute real path.
   * @return true if succeed. false if the DOM path is invalid.
   */
  bool
  DOMPathToRealPath(const nsAString& aPath, nsAString& aRealPath) const;

  already_AddRefed<Promise>
  RemoveInternal(const StringOrFileOrDirectory& aPath, bool aRecursive);

  nsRefPtr<FileSystemBase> mFileSystem;
  nsString mPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Directory_h
