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
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

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

  static already_AddRefed<Directory>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aRealPath,
              ErrorResult& aRv);

  static already_AddRefed<Directory>
  Create(nsISupports* aParent, nsIFile* aDirectory,
         FileSystemBase* aFileSystem = 0);

  // ========= Begin WebIDL bindings. ===========

  nsISupports*
  GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetName(nsAString& aRetval, ErrorResult& aRv);

  // From https://microsoftedge.github.io/directory-upload/proposal.html#directory-interface :

  void
  GetPath(nsAString& aRetval, ErrorResult& aRv);

  nsresult
  GetFullRealPath(nsAString& aPath);

  already_AddRefed<Promise>
  GetFilesAndDirectories(ErrorResult& aRv);

  already_AddRefed<Promise>
  GetFiles(bool aRecursiveFlag, ErrorResult& aRv);

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
  GetFileSystem(ErrorResult& aRv);

  nsIFile*
  GetInternalNsIFile() const
  {
    return mFile;
  }

private:
  Directory(nsISupports* aParent,
            nsIFile* aFile,
            FileSystemBase* aFileSystem = nullptr);
  ~Directory();

  /*
   * Convert relative DOM path to the absolute real path.
   */
  nsresult
  DOMPathToRealPath(const nsAString& aPath, nsIFile** aFile) const;

  nsCOMPtr<nsISupports> mParent;
  RefPtr<FileSystemBase> mFileSystem;
  nsCOMPtr<nsIFile> mFile;

  nsString mFilters;
  nsString mPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Directory_h
