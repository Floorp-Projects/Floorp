/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filehandle_ActorsChild_h
#define mozilla_dom_filehandle_ActorsChild_h

#include "js/RootingAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FileHandleCommon.h"
#include "mozilla/dom/PBackgroundFileHandleChild.h"
#include "mozilla/dom/PBackgroundFileRequestChild.h"
#include "mozilla/dom/PBackgroundMutableFileChild.h"

class nsCString;

namespace mozilla {
namespace dom {

class FileHandleBase;
class FileRequestBase;
class MutableFileBase;

class BackgroundMutableFileChildBase
  : public ThreadObject
  , public PBackgroundMutableFileChild
{
protected:
  friend class MutableFileBase;

  RefPtr<MutableFileBase> mTemporaryStrongMutableFile;
  MutableFileBase* mMutableFile;

public:
  void
  EnsureDOMObject();

  MutableFileBase*
  GetDOMObject() const
  {
    AssertIsOnOwningThread();
    return mMutableFile;
  }

  void
  ReleaseDOMObject();

protected:
  BackgroundMutableFileChildBase(DEBUGONLY(PRThread* aOwningThread));

  ~BackgroundMutableFileChildBase();

  void
  SendDeleteMeInternal();

  virtual already_AddRefed<MutableFileBase>
  CreateMutableFile() = 0;

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PBackgroundFileHandleChild*
  AllocPBackgroundFileHandleChild(const FileMode& aMode) override;

  virtual bool
  DeallocPBackgroundFileHandleChild(PBackgroundFileHandleChild* aActor)
                                    override;

  bool
  SendDeleteMe() = delete;
};

class BackgroundFileHandleChild
  : public ThreadObject
  , public PBackgroundFileHandleChild
{
  friend class BackgroundMutableFileChildBase;
  friend class MutableFileBase;

  // mTemporaryStrongFileHandle is strong and is only valid until the end of
  // NoteComplete() member function or until the NoteActorDestroyed() member
  // function is called.
  RefPtr<FileHandleBase> mTemporaryStrongFileHandle;

  // mFileHandle is weak and is valid until the NoteActorDestroyed() member
  // function is called.
  FileHandleBase* mFileHandle;

public:
  explicit BackgroundFileHandleChild(DEBUGONLY(PRThread* aOwningThread,)
                                     FileHandleBase* aFileHandle);

  void
  SendDeleteMeInternal();

private:
  ~BackgroundFileHandleChild();

  void
  NoteActorDestroyed();

  void
  NoteComplete();

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  bool
  RecvComplete(const bool& aAborted) override;

  virtual PBackgroundFileRequestChild*
  AllocPBackgroundFileRequestChild(const FileRequestParams& aParams)
                                   override;

  virtual bool
  DeallocPBackgroundFileRequestChild(PBackgroundFileRequestChild* aActor)
                                     override;

  bool
  SendDeleteMe() = delete;
};

class BackgroundFileRequestChild final
  : public ThreadObject
  , public PBackgroundFileRequestChild
{
  friend class BackgroundFileHandleChild;
  friend class FileHandleBase;

  RefPtr<FileRequestBase> mFileRequest;
  RefPtr<FileHandleBase> mFileHandle;
  bool mActorDestroyed;

private:
  // Only created by FileHandleBase.
  explicit BackgroundFileRequestChild(DEBUGONLY(PRThread* aOwningThread,)
                                      FileRequestBase* aFileRequest);

  // Only destroyed by BackgroundFileHandleChild.
  ~BackgroundFileRequestChild();

  void
  HandleResponse(nsresult aResponse);

  void
  HandleResponse(const FileRequestGetFileResponse& aResponse);

  void
  HandleResponse(const nsCString& aResponse);

  void
  HandleResponse(const FileRequestMetadata& aResponse);

  void
  HandleResponse(JS::Handle<JS::Value> aResponse);

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__(const FileRequestResponse& aResponse) override;

  virtual bool
  RecvProgress(const uint64_t& aProgress,
               const uint64_t& aProgressMax) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_filehandle_ActorsChild_h
