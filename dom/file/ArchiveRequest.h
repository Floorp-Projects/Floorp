/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domarchiverequest_h__
#define mozilla_dom_file_domarchiverequest_h__

#include "mozilla/Attributes.h"
#include "ArchiveReader.h"
#include "DOMRequest.h"

#include "FileCommon.h"


BEGIN_FILE_NAMESPACE

/**
 * This is the ArchiveRequest that handles any operation
 * related to ArchiveReader
 */
class ArchiveRequest : public mozilla::dom::DOMRequest
{
public:
  static bool PrefEnabled()
  {
    return ArchiveReader::PrefEnabled();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  ArchiveReader* Reader() const;

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ArchiveRequest, DOMRequest)

  ArchiveRequest(nsIDOMWindow* aWindow,
                 ArchiveReader* aReader);

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

public:
  // This is called by the DOMArchiveRequestEvent
  void Run();

  // Set the types for this request
  void OpGetFilenames();
  void OpGetFile(const nsAString& aFilename);
  void OpGetFiles();

  nsresult ReaderReady(nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList,
                       nsresult aStatus);

public: // static
  static already_AddRefed<ArchiveRequest> Create(nsIDOMWindow* aOwner,
                                                 ArchiveReader* aReader);

private:
  ~ArchiveRequest();

  nsresult GetFilenamesResult(JSContext* aCx,
                              JS::Value* aValue,
                              nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList);
  nsresult GetFileResult(JSContext* aCx,
                         JS::MutableHandle<JS::Value> aValue,
                         nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList);
  nsresult GetFilesResult(JSContext* aCx,
                          JS::MutableHandle<JS::Value> aValue,
                          nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList);

protected:
  // The reader:
  nsRefPtr<ArchiveReader> mArchiveReader;

  // The operation:
  enum {
    GetFilenames,
    GetFile,
    GetFiles
  } mOperation;

  // The filename (needed by GetFile):
  nsString mFilename;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchiverequest_h__
