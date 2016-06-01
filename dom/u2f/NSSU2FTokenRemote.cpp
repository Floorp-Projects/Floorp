/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"

#include "NSSU2FTokenRemote.h"

using mozilla::dom::ContentChild;

NS_IMPL_ISUPPORTS(NSSU2FTokenRemote, nsINSSU2FToken)

static mozilla::LazyLogModule gWebauthLog("webauth_u2f");

NSSU2FTokenRemote::NSSU2FTokenRemote()
{}

NSSU2FTokenRemote::~NSSU2FTokenRemote()
{}

NS_IMETHODIMP
NSSU2FTokenRemote::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
NSSU2FTokenRemote::IsCompatibleVersion(const nsAString& aVersionString,
                                       bool* aIsCompatible)
{
  NS_ENSURE_ARG_POINTER(aIsCompatible);

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsCompatibleVersion(
        nsString(aVersionString), aIsCompatible)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
NSSU2FTokenRemote::IsRegistered(uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                                bool* aIsRegistered)
{
  NS_ENSURE_ARG_POINTER(aKeyHandle);
  NS_ENSURE_ARG_POINTER(aIsRegistered);

  nsTArray<uint8_t> keyHandle;
  if (!keyHandle.ReplaceElementsAt(0, keyHandle.Length(), aKeyHandle,
                                   aKeyHandleLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsRegistered(keyHandle, aIsRegistered)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
NSSU2FTokenRemote::Register(uint8_t* aApplication,
                            uint32_t aApplicationLen,
                            uint8_t* aChallenge,
                            uint32_t aChallengeLen,
                            uint8_t** aRegistration,
                            uint32_t* aRegistrationLen)
{
  NS_ENSURE_ARG_POINTER(aApplication);
  NS_ENSURE_ARG_POINTER(aChallenge);
  NS_ENSURE_ARG_POINTER(aRegistration);
  NS_ENSURE_ARG_POINTER(aRegistrationLen);

  nsTArray<uint8_t> application;
  if (!application.ReplaceElementsAt(0, application.Length(), aApplication,
                                     aApplicationLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsTArray<uint8_t> challenge;
  if (!challenge.ReplaceElementsAt(0, challenge.Length(), aChallenge,
                                   aChallengeLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsTArray<uint8_t> registrationBuffer;
  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenRegister(application, challenge,
                                   &registrationBuffer)) {
    return NS_ERROR_FAILURE;
  }

  size_t dataLen = registrationBuffer.Length();
  uint8_t* tmp = reinterpret_cast<uint8_t*>(moz_xmalloc(dataLen));
  if (NS_WARN_IF(!tmp)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy(tmp, registrationBuffer.Elements(), dataLen);
  *aRegistration = tmp;
  *aRegistrationLen = dataLen;
  return NS_OK;
}

NS_IMETHODIMP
NSSU2FTokenRemote::Sign(uint8_t* aApplication, uint32_t aApplicationLen,
                        uint8_t* aChallenge, uint32_t aChallengeLen,
                        uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                        uint8_t** aSignature, uint32_t* aSignatureLen)
{
  NS_ENSURE_ARG_POINTER(aApplication);
  NS_ENSURE_ARG_POINTER(aChallenge);
  NS_ENSURE_ARG_POINTER(aKeyHandle);
  NS_ENSURE_ARG_POINTER(aSignature);
  NS_ENSURE_ARG_POINTER(aSignatureLen);

  nsTArray<uint8_t> application;
  if (!application.ReplaceElementsAt(0, application.Length(), aApplication,
                                     aApplicationLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsTArray<uint8_t> challenge;
  if (!challenge.ReplaceElementsAt(0, challenge.Length(), aChallenge,
                                   aChallengeLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsTArray<uint8_t> keyHandle;
  if (!keyHandle.ReplaceElementsAt(0, keyHandle.Length(), aKeyHandle,
                                   aKeyHandleLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsTArray<uint8_t> signatureBuffer;
  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenSign(application, challenge, keyHandle,
                               &signatureBuffer)) {
    return NS_ERROR_FAILURE;
  }

  size_t dataLen = signatureBuffer.Length();
  uint8_t* tmp = reinterpret_cast<uint8_t*>(moz_xmalloc(dataLen));
  if (NS_WARN_IF(!tmp)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy(tmp, signatureBuffer.Elements(), dataLen);
  *aSignature = tmp;
  *aSignatureLen = dataLen;
  return NS_OK;
}
