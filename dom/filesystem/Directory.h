/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Directory_h
#define mozilla_dom_Directory_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/File.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

// Resolve the name collision of Microsoft's API name with macros defined in
// Windows header files. Undefine the macro of CreateDirectory to avoid
// Directory#CreateDirectory being replaced by Directory#CreateDirectoryW.
#ifdef CreateDirectory
#undef CreateDirectory
#endif
// Undefine the macro of CreateFile to avoid Directory#CreateFile being replaced
// by Directory#CreateFileW.
#ifdef CreateFile
#undef CreateFile
#endif

namespace mozilla {
namespace dom {

struct CreateFileOptions;
class FileSystemBase;
class Promise;
class StringOrFileOrDirectory;

class Directory final
  : public nsISupports
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Directory)

public:
  static already_AddRefed<Promise>
  GetRoot(FileSystemBase* aFileSystem, ErrorResult& aRv);

  Directory(FileSystemBase* aFileSystem, const nsAString& aPath);

  // ========= Begin WebIDL bindings. ===========

  nsPIDOMWindowInner*
  GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetName(nsAString& aRetval) const;

  already_AddRefed<Promise>
  CreateFile(const nsAString& aPath, const CreateFileOptions& aOptions,
             ErrorResult& aRv);

  already_AddRefed<Promise>
  CreateDirectory(const nsAString& aPath, ErrorResult& aRv);

  already_AddRefed<Promise>
  Get(const nsAString& aPath, ErrorResult& aRv);

  already_AddRefed<Promise>
  Remove(const StringOrFileOrDirectory& aPath, ErrorResult& aRv);

  already_AddRefed<Promise>
  RemoveDeep(const StringOrFileOrDirectory& aPath, ErrorResult& aRv);

  // From https://microsoftedge.github.io/directory-upload/proposal.html#directory-interface :

  void
  GetPath(nsAString& aRetval) const;

  already_AddRefed<Promise>
  GetFilesAndDirectories();

  // =========== End WebIDL bindings.============

  /**
   * Sets a semi-colon separated list of filters to filter-in or filter-out
   * certain types of files when the contents of this directory are requested
   * via a GetFilesAndDirectories() call.
   *
   * Currently supported keywords:
   *
   *   * filter-out-sensitive
   *       This keyword filters out files or directories that we don't wish to
   *       make available to Web content because we are concerned that there is
   *       a risk that users may unwittingly give Web content access to them
   *       and suffer undesirable consequences.  The details of what is
   *       filtered out can be found in GetDirectoryListingTask::Work.
   *
   * In future, we will likely support filtering based on filename extensions
   * (for example, aFilters could be "*.jpg; *.jpeg; *.gif"), but that isn't
   * supported yet.  Once supported, files that don't match a specified
   * extension (if any are specified) would be filtered out.  This
   * functionality would allow us to apply the 'accept' attribute from
   * <input type=file directory accept="..."> to the results of a directory
   * picker operation.
   */
  void
  SetContentFilters(const nsAString& aFilters);

  FileSystemBase*
  GetFileSystem() const;
private:
  ~Directory();

  static bool
  IsValidRelativePath(const nsString& aPath);

  /*
   * Convert relative DOM path to the absolute real path.
   * @return true if succeed. false if the DOM path is invalid.
   */
  bool
  DOMPathToRealPath(const nsAString& aPath, nsAString& aRealPath) const;

  already_AddRefed<Promise>
  RemoveInternal(const StringOrFileOrDirectory& aPath, bool aRecursive,
                 ErrorResult& aRv);

  RefPtr<FileSystemBase> mFileSystem;
  nsString mPath;
  nsString mFilters;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Directory_h
