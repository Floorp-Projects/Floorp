/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CDMProxy.h"
#include "nsString.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "MainThreadUtils.h"

// TODO: Change the functions in this file to do IPC via the Gecko Media
// Plugins API. In the meantime, the code here merely implements the
// interface we expect will be required when the IPC is working.

namespace mozilla {

CDMProxy::CDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem)
  : mKeys(aKeys)
  , mKeySystem(aKeySystem)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(CDMProxy);
}

CDMProxy::~CDMProxy()
{
  MOZ_COUNT_DTOR(CDMProxy);
}

void
CDMProxy::Init(PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps =
      do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    if (!mps) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    mps->GetThread(getter_AddRefs(mGMPThread));
    if (!mGMPThread) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
  }

  // TODO: Dispatch task to GMPThread to initialize CDM via IPC.

  mKeys->OnCDMCreated(aPromiseId);
}

static int sFakeSessionIdNum = 0;

void
CDMProxy::CreateSession(dom::SessionType aSessionType,
                        PromiseId aPromiseId,
                        const nsAString& aInitDataType,
                        const Uint8Array& aInitData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  // TODO: Dispatch task to GMPThread to call CDM CreateSession via IPC.

  // Make a fake session id. We'll get this from the CDM normally.
  nsAutoString id;
  id.AppendASCII("FakeSessionId_");
  id.AppendInt(sFakeSessionIdNum++);

  mKeys->OnSessionActivated(aPromiseId, id);
}

void
CDMProxy::LoadSession(PromiseId aPromiseId,
                      const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  // TODO: Dispatch task to GMPThread to call CDM LoadSession via IPC.
  // make MediaKeys::mPendingSessions CC'd

  mKeys->OnSessionActivated(aPromiseId, aSessionId);
}

void
CDMProxy::SetServerCertificate(PromiseId aPromiseId,
                               const Uint8Array& aCertData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  // TODO: Dispatch task to GMPThread to call CDM SetServerCertificate via IPC.

  ResolvePromise(aPromiseId);
}

static int sUpdateCount = 0;

void
CDMProxy::UpdateSession(const nsAString& aSessionId,
                        PromiseId aPromiseId,
                        const Uint8Array& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  // TODO: Dispatch task to GMPThread to call CDM UpdateSession via IPC.

  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  nsAutoCString str(NS_LITERAL_CSTRING("Update_"));
  str.AppendInt(sUpdateCount++);
  nsTArray<uint8_t> msg;
  msg.AppendElements(str.get(), str.Length());
  session->DispatchKeyMessage(msg, NS_LITERAL_STRING("http://bogus.url"));
  ResolvePromise(aPromiseId);
}

void
CDMProxy::CloseSession(const nsAString& aSessionId,
                       PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  // TODO: Dispatch task to GMPThread to call CDM CloseSession via IPC.

  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));

  // Pretend that the CDM actually does close the session...
  // "...the [MediaKeySession's] closed attribute promise is resolved
  // when the session is closed."
  session->OnClosed();

  // "The promise is resolved when the request has been processed."
  ResolvePromise(aPromiseId);
}

void
CDMProxy::RemoveSession(const nsAString& aSessionId,
                        PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Dispatch task to GMPThread to call CDM RemoveSession via IPC.

  // Assume CDM immediately removes session's data, then close the session
  // as per the spec.
  CloseSession(aSessionId, aPromiseId);
}

void
CDMProxy::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mKeys.Clear();
}

void
CDMProxy::RejectPromise(PromiseId aId, nsresult aCode)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->RejectPromise(aId, aCode);
    } else {
      NS_WARNING("CDMProxy unable to reject promise!");
    }
  } else {
    nsRefPtr<nsIRunnable> task(new RejectPromiseTask(this, aId, aCode));
    NS_DispatchToMainThread(task);
  }
}

void
CDMProxy::ResolvePromise(PromiseId aId)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aId);
    } else {
      NS_WARNING("CDMProxy unable to resolve promise!");
    }
  } else {
    nsRefPtr<nsIRunnable> task;
    task = NS_NewRunnableMethodWithArg<PromiseId>(this,
                                                  &CDMProxy::ResolvePromise,
                                                  aId);
    NS_DispatchToMainThread(task);
  }
}

} // namespace mozilla
