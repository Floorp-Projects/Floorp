/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
#include "ClearKeyStorage.h"
#include "gmp-task-utils.h"

#include "gmp-api/gmp-decryption.h"
#include "mozilla/Endian.h"
#include "pk11pub.h"

using namespace mozilla;

ClearKeySession::ClearKeySession(const std::string& aSessionId,
                                 GMPDecryptorCallback* aCallback,
                                 GMPSessionType aSessionType)
  : mSessionId(aSessionId)
  , mCallback(aCallback)
  , mSessionType(aSessionType)
{
  CK_LOGD("ClearKeySession ctor %p", this);
}

ClearKeySession::~ClearKeySession()
{
  CK_LOGD("ClearKeySession dtor %p", this);
}

void
ClearKeySession::Init(uint32_t aPromiseId,
                      const uint8_t* aInitData, uint32_t aInitDataSize)
{
  CK_LOGD("ClearKeySession::Init");

  ClearKeyUtils::ParseInitData(aInitData, aInitDataSize, mKeyIds);
  if (!mKeyIds.size()) {
    const char message[] = "Couldn't parse cenc key init data";
    mCallback->RejectPromise(aPromiseId, kGMPAbortError, message, strlen(message));
    return;
  }
  mCallback->ResolveNewSessionPromise(aPromiseId,
                                      mSessionId.data(), mSessionId.length());
}

GMPSessionType
ClearKeySession::Type() const
{
  return mSessionType;
}

void
ClearKeySession::AddKeyId(const KeyId& aKeyId)
{
  mKeyIds.push_back(aKeyId);
}
