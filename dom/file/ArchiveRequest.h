/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domarchiverequest_h__
#define mozilla_dom_file_domarchiverequest_h__

#include "nsIDOMArchiveRequest.h"
#include "ArchiveReader.h"
#include "DOMRequest.h"

#include "FileCommon.h"


BEGIN_FILE_NAMESPACE

class ArchiveRequest : public mozilla::dom::DOMRequest,
                       public nsIDOMArchiveRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMARCHIVEREQUEST

  NS_FORWARD_NSIDOMDOMREQUEST(DOMRequest::)
  NS_FORWARD_NSIDOMEVENTTARGET_NOPREHANDLEEVENT(DOMRequest::)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ArchiveRequest, DOMRequest)

  ArchiveRequest(nsIDOMWindow* aWindow,
                 ArchiveReader* aReader);

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

public:
  // This is called by the DOMArchiveRequestEvent
  void Run();

  // Set the types for this request
  void OpGetFilenames();
  void OpGetFile(const nsAString& aFilename);

  nsresult ReaderReady(nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList,
                       nsresult aStatus);

public: // static
  static already_AddRefed<ArchiveRequest> Create(nsIDOMWindow* aOwner,
                                                 ArchiveReader* aReader);

private:
  ~ArchiveRequest();

  nsresult GetFilenamesResult(JSContext* aCx,
                              jsval* aValue,
                              nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList);
  nsresult GetFileResult(JSContext* aCx,
                         jsval* aValue,
                         nsTArray<nsCOMPtr<nsIDOMFile> >& aFileList);

protected:
  // The reader:
  nsRefPtr<ArchiveReader> mArchiveReader;

  // The operation:
  enum {
    GetFilenames,
    GetFile
  } mOperation;

  // The filename (needed by GetFile):
  nsString mFilename;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchiverequest_h__
