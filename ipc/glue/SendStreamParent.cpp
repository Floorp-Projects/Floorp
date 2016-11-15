/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SendStream.h"

#include "mozilla/Unused.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"

namespace mozilla {
namespace ipc {

namespace {

class SendStreamParentImpl final : public SendStreamParent
{
public:
  SendStreamParentImpl(nsIAsyncInputStream* aReader,
                        nsIAsyncOutputStream* aWriter);
  ~SendStreamParentImpl();

private:
  // PSendStreamParentImpl methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  // SendStreamparent methods
  already_AddRefed<nsIInputStream>
  TakeReader() override;

  virtual mozilla::ipc::IPCResult
  RecvBuffer(const nsCString& aBuffer) override;

  virtual mozilla::ipc::IPCResult
  RecvClose(const nsresult& aRv) override;

  nsCOMPtr<nsIAsyncInputStream> mReader;
  nsCOMPtr<nsIAsyncOutputStream> mWriter;

  NS_DECL_OWNINGTHREAD
};

SendStreamParentImpl::~SendStreamParentImpl()
{
}

already_AddRefed<nsIInputStream>
SendStreamParentImpl::TakeReader()
{
  MOZ_ASSERT(mReader);
  return mReader.forget();
}

void
SendStreamParentImpl::ActorDestroy(ActorDestroyReason aReason)
{
  // If we were gracefully closed we should have gotten RecvClose().  In
  // that case, the writer will already be closed and this will have no
  // effect.  This just aborts the writer in the case where the child process
  // crashes.
  mWriter->CloseWithStatus(NS_ERROR_ABORT);
}

mozilla::ipc::IPCResult
SendStreamParentImpl::RecvBuffer(const nsCString& aBuffer)
{
  uint32_t numWritten = 0;

  // This should only fail if we hit an OOM condition.
  nsresult rv = mWriter->Write(aBuffer.get(), aBuffer.Length(), &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendRequestClose(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
SendStreamParentImpl::RecvClose(const nsresult& aRv)
{
  mWriter->CloseWithStatus(aRv);
  Unused << Send__delete__(this);
  return IPC_OK();
}

SendStreamParentImpl::SendStreamParentImpl(nsIAsyncInputStream* aReader,
                                             nsIAsyncOutputStream* aWriter)
  : mReader(aReader)
  , mWriter(aWriter)
{
  MOZ_ASSERT(mReader);
  MOZ_ASSERT(mWriter);
}

} // anonymous namespace

SendStreamParent::~SendStreamParent()
{
}

PSendStreamParent*
AllocPSendStreamParent()
{
  // use async versions for both reader and writer even though we are
  // opening the writer as an infinite stream.  We want to be able to
  // use CloseWithStatus() to communicate errors through the pipe.
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  // Use an "infinite" pipe because we cannot apply back-pressure through
  // the async IPC layer at the moment.  Blocking the IPC worker thread
  // is not desirable, either.
  nsresult rv = NS_NewPipe2(getter_AddRefs(reader),
                            getter_AddRefs(writer),
                            true, true,   // non-blocking
                            0,            // segment size
                            UINT32_MAX);  // "infinite" pipe
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return new SendStreamParentImpl(reader, writer);
}

void
DeallocPSendStreamParent(PSendStreamParent* aActor)
{
  delete aActor;
}

} // namespace ipc
} // namespace mozilla
