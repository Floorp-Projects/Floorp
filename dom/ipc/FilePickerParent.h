/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FilePickerParent_h
#define mozilla_dom_FilePickerParent_h

#include "nsIEventTarget.h"
#include "nsIFilePicker.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PFilePickerParent.h"

class nsIFile;

namespace mozilla::dom {

class FilePickerParent : public PFilePickerParent {
 public:
  FilePickerParent(const nsString& aTitle, const nsIFilePicker::Mode& aMode)
      : mTitle(aTitle), mMode(aMode), mResult(nsIFilePicker::returnOK) {}

 private:
  virtual ~FilePickerParent();

 public:
  NS_INLINE_DECL_REFCOUNTING(FilePickerParent, final)

  void Done(nsIFilePicker::ResultCode aResult);

  struct BlobImplOrString {
    RefPtr<BlobImpl> mBlobImpl;
    nsString mDirectoryPath;

    enum { eBlobImpl, eDirectoryPath } mType;
  };

  void SendFilesOrDirectories(const nsTArray<BlobImplOrString>& aData);

  mozilla::ipc::IPCResult RecvOpen(
      const int16_t& aSelectedType, const bool& aAddToRecentDocs,
      const nsString& aDefaultFile, const nsString& aDefaultExtension,
      nsTArray<nsString>&& aFilters, nsTArray<nsString>&& aFilterNames,
      nsTArray<nsString>&& aRawFilters, const nsString& aDisplayDirectory,
      const nsString& aDisplaySpecialDirectory, const nsString& aOkButtonLabel,
      const nsIFilePicker::CaptureTarget& aCapture);

  mozilla::ipc::IPCResult RecvClose();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  class FilePickerShownCallback : public nsIFilePickerShownCallback {
   public:
    explicit FilePickerShownCallback(FilePickerParent* aFilePickerParent)
        : mFilePickerParent(aFilePickerParent) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFILEPICKERSHOWNCALLBACK

    void Destroy();

   private:
    virtual ~FilePickerShownCallback() = default;
    RefPtr<FilePickerParent> mFilePickerParent;
  };

 private:
  bool CreateFilePicker();

  // This runnable is used to do some I/O operation on a separate thread.
  class IORunnable : public Runnable {
    RefPtr<FilePickerParent> mFilePickerParent;
    nsTArray<nsCOMPtr<nsIFile>> mFiles;
    nsTArray<BlobImplOrString> mResults;
    nsCOMPtr<nsIEventTarget> mEventTarget;
    bool mIsDirectory;

   public:
    IORunnable(FilePickerParent* aFPParent,
               nsTArray<nsCOMPtr<nsIFile>>&& aFiles, bool aIsDirectory);

    bool Dispatch();
    NS_IMETHOD Run() override;
    void Destroy();
  };

  RefPtr<IORunnable> mRunnable;
  RefPtr<FilePickerShownCallback> mCallback;
  nsCOMPtr<nsIFilePicker> mFilePicker;

  nsString mTitle;
  nsIFilePicker::Mode mMode;
  nsIFilePicker::ResultCode mResult;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FilePickerParent_h
