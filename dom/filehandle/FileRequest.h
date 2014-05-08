/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileRequest_h
#define mozilla_dom_FileRequest_h

#include "DOMRequest.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindow;

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class FileHelper;
class LockedFile;

class FileRequest : public DOMRequest
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

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileRequest_h
