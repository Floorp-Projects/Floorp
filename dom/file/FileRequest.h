/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_filerequest_h__
#define mozilla_dom_file_filerequest_h__

#include "mozilla/Attributes.h"
#include "FileCommon.h"

#include "DOMRequest.h"

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

BEGIN_FILE_NAMESPACE

class FileHelper;
class LockedFile;

class FileRequest : public mozilla::dom::DOMRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileRequest, DOMRequest)

  static already_AddRefed<FileRequest>
  Create(nsPIDOMWindow* aOwner, LockedFile* aLockedFile,
         bool aWrapAsDOMRequest);

  // nsIDOMEventTarget
  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  void
  OnProgress(uint64_t aProgress, uint64_t aProgressMax)
  {
    FireProgressEvent(aProgress, aProgressMax);
  }

  nsresult
  NotifyHelperCompleted(FileHelper* aFileHelper);

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  LockedFile*
  GetLockedFile() const;

  IMPL_EVENT_HANDLER(progress)

protected:
  FileRequest(nsPIDOMWindow* aWindow);
  ~FileRequest();

  void
  FireProgressEvent(uint64_t aLoaded, uint64_t aTotal);

  nsRefPtr<LockedFile> mLockedFile;

  bool mWrapAsDOMRequest;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_filerequest_h__
