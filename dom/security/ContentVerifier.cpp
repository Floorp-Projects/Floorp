/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentVerifier.h"

#include "mozilla/fallible.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "nssb64.h"
#include "nsSecurityHeaderParser.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"

using namespace mozilla;

static LazyLogModule gContentVerifierPRLog("ContentVerifier");
#define CSV_LOG(args) MOZ_LOG(gContentVerifierPRLog, LogLevel::Debug, args)

// Content-Signature prefix
const nsLiteralCString kPREFIX = NS_LITERAL_CSTRING("Content-Signature:\x00");

NS_IMPL_ISUPPORTS(ContentVerifier, nsIStreamListener, nsISupports);

nsresult
ContentVerifier::Init(const nsAString& aContentSignatureHeader)
{
  mVks = Preferences::GetString("browser.newtabpage.remote.keys");

  if (aContentSignatureHeader.IsEmpty() || mVks.IsEmpty()) {
    CSV_LOG(
      ("Content-Signature header and verification keys must not be empty!\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  nsresult rv = ParseContentSignatureHeader(aContentSignatureHeader);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_SIGNATURE);
  return CreateContext();
}

/**
 * Implement nsIStreamListener
 * We buffer the entire content here and kick off verification
 */
NS_METHOD
AppendNextSegment(nsIInputStream* aInputStream, void* aClosure,
                  const char* aRawSegment, uint32_t aToOffset, uint32_t aCount,
                  uint32_t* outWrittenCount)
{
  FallibleTArray<nsCString>* decodedData =
    static_cast<FallibleTArray<nsCString>*>(aClosure);
  nsAutoCString segment(aRawSegment, aCount);
  if (!decodedData->AppendElement(segment, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *outWrittenCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
ContentVerifier::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
ContentVerifier::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
  // Verify the content:
  // If this fails, we return an invalid signature error to load a fallback page.
  // If everthing is good, we return a new stream to the next listener and kick
  // that one of.
  CSV_LOG(("VerifySignedContent, b64signature: %s\n", mSignature.get()));
  CSV_LOG(("VerifySignedContent, key: \n[\n%s\n]\n", mKey.get()));
  bool verified = false;
  nsresult rv = End(&verified);
  if (NS_FAILED(rv) || !verified || NS_FAILED(aStatus)) {
    // cancel the request and return error
    if (NS_FAILED(aStatus)) {
      rv = aStatus;
    } else {
      rv = NS_ERROR_INVALID_SIGNATURE;
    }
    CSV_LOG(("failed to verify content\n"));
    mNextListener->OnStartRequest(aRequest, aContext);
    mNextListener->OnStopRequest(aRequest, aContext, rv);
    return NS_ERROR_INVALID_SIGNATURE;
  }
  CSV_LOG(("Successfully verified content signature.\n"));

  // start the next listener
  rv = mNextListener->OnStartRequest(aRequest, aContext);
  if (NS_SUCCEEDED(rv)) {
    // We emptied aInStr so we have to create a new one from buf to hand it
    // to the consuming listener.
    for (uint32_t i = 0; i < mContent.Length(); ++i) {
      nsCOMPtr<nsIInputStream> oInStr;
      rv = NS_NewCStringInputStream(getter_AddRefs(oInStr), mContent[i]);
      if (NS_FAILED(rv)) {
        break;
      }
      // let the next listener know that there is data in oInStr
      rv = mNextListener->OnDataAvailable(aRequest, aContext, oInStr, 0,
                                          mContent[i].Length());
      if (NS_FAILED(rv)) {
        break;
      }
    }
  }

  // propagate OnStopRequest and return
  return mNextListener->OnStopRequest(aRequest, aContext, rv);
}

NS_IMETHODIMP
ContentVerifier::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                 nsIInputStream* aInputStream, uint64_t aOffset,
                                 uint32_t aCount)
{
  // buffer the entire stream
  uint32_t read;
  nsresult rv = aInputStream->ReadSegments(AppendNextSegment, &mContent, aCount,
                                           &read);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // update the signature verifier
  return Update(mContent[mContent.Length()-1]);
}

/**
 * ContentVerifier logic and utils
 */

nsresult
ContentVerifier::GetVerificationKey(const nsAString& aKeyId)
{
  // get verification keys from the pref and see if we have |aKeyId|
  nsCharSeparatedTokenizer tokenizerVK(mVks, ';');
  while (tokenizerVK.hasMoreTokens()) {
    nsDependentSubstring token = tokenizerVK.nextToken();
    nsCharSeparatedTokenizer tokenizerKey(token, '=');
    nsString prefKeyId;
    if (tokenizerKey.hasMoreTokens()) {
      prefKeyId = tokenizerKey.nextToken();
    }
    nsString key;
    if (tokenizerKey.hasMoreTokens()) {
      key = tokenizerKey.nextToken();
    }
    if (prefKeyId.Equals(aKeyId)) {
      mKey.Assign(NS_ConvertUTF16toUTF8(key));
      return NS_OK;
    }
  }

  // we didn't find the appropriate key
  return NS_ERROR_INVALID_SIGNATURE;
}

nsresult
ContentVerifier::ParseContentSignatureHeader(
  const nsAString& aContentSignatureHeader)
{
  // We only support p384 ecdsa according to spec
  NS_NAMED_LITERAL_CSTRING(keyid_var, "keyid");
  NS_NAMED_LITERAL_CSTRING(signature_var, "p384ecdsa");

  nsAutoString contentSignature;
  nsAutoString keyId;
  nsAutoCString header = NS_ConvertUTF16toUTF8(aContentSignatureHeader);
  nsSecurityHeaderParser parser(header.get());
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    CSV_LOG(("ContentVerifier: could not parse ContentSignature header\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }
  LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    CSV_LOG(("ContentVerifier: found directive %s\n", directive->mName.get()));
    if (directive->mName.Length() == keyid_var.Length() &&
        directive->mName.EqualsIgnoreCase(keyid_var.get(),
                                          keyid_var.Length())) {
      if (!keyId.IsEmpty()) {
        CSV_LOG(("ContentVerifier: found two keyIds\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSV_LOG(("ContentVerifier: found a keyid directive\n"));
      keyId = NS_ConvertUTF8toUTF16(directive->mValue);
      rv = GetVerificationKey(keyId);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_SIGNATURE);
    }
    if (directive->mName.Length() == signature_var.Length() &&
        directive->mName.EqualsIgnoreCase(signature_var.get(),
                                          signature_var.Length())) {
      if (!contentSignature.IsEmpty()) {
        CSV_LOG(("ContentVerifier: found two ContentSignatures\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSV_LOG(("ContentVerifier: found a ContentSignature directive\n"));
      contentSignature = NS_ConvertUTF8toUTF16(directive->mValue);
      mSignature = directive->mValue;
    }
  }

  // we have to ensure that we found a key and a signature at this point
  if (mKey.IsEmpty()) {
    CSV_LOG(("ContentVerifier: got a Content-Signature header but didn't find "
             "an appropriate key.\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }
  if (mSignature.IsEmpty()) {
    CSV_LOG(("ContentVerifier: got a Content-Signature header but didn't find "
             "a signature.\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  return NS_OK;
}

/**
 * Parse signature, public key, and algorithm data for input to verification
 * functions in VerifyData and CreateContext.
 *
 * https://datatracker.ietf.org/doc/draft-thomson-http-content-signature/
 * If aSignature is a content signature, the function returns
 * NS_ERROR_INVALID_SIGNATURE if anything goes wrong. Only p384 with sha384
 * is supported and aSignature is a raw signature (r||s).
 */
nsresult
ContentVerifier::ParseInput(ScopedSECKEYPublicKey& aPublicKeyOut,
                            ScopedSECItem& aSignatureItemOut,
                            SECOidTag& aOidOut,
                            const nsNSSShutDownPreventionLock&)
{
  // Base 64 decode the key
  ScopedSECItem keyItem(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (!keyItem ||
      !NSSBase64_DecodeBuffer(nullptr, keyItem,
                              mKey.get(),
                              mKey.Length())) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Extract the public key from the keyItem
  ScopedCERTSubjectPublicKeyInfo pki(
    SECKEY_DecodeDERSubjectPublicKeyInfo(keyItem));
  if (!pki) {
    return NS_ERROR_INVALID_SIGNATURE;
  }
  aPublicKeyOut = SECKEY_ExtractPublicKey(pki.get());

  // in case we were not able to extract a key
  if (!aPublicKeyOut) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Base 64 decode the signature
  ScopedSECItem rawSignatureItem(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (!rawSignatureItem ||
      !NSSBase64_DecodeBuffer(nullptr, rawSignatureItem,
                              mSignature.get(),
                              mSignature.Length())) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // get signature object and oid
  if (!aSignatureItemOut) {
    return NS_ERROR_INVALID_SIGNATURE;
  }
  // We have a raw ecdsa signature r||s so we have to DER-encode it first
  // Note that we have to check rawSignatureItem->len % 2 here as
  // DSAU_EncodeDerSigWithLen asserts this
  if (rawSignatureItem->len == 0 || rawSignatureItem->len % 2 != 0) {
    return NS_ERROR_INVALID_SIGNATURE;
  }
  if (DSAU_EncodeDerSigWithLen(aSignatureItemOut, rawSignatureItem,
                               rawSignatureItem->len) != SECSuccess) {
    return NS_ERROR_INVALID_SIGNATURE;
  }
  aOidOut = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;

  return NS_OK;
}

/**
 * Create a context for a signature verification.
 * It sets signature, public key, and algorithms that should be used to verify
 * the data. It also updates the verification buffer with the content-signature
 * prefix.
 */
nsresult
ContentVerifier::CreateContext()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Bug 769521: We have to change b64 url to regular encoding as long as we
  // don't have a b64 url decoder. This should change soon, but in the meantime
  // we have to live with this.
  mSignature.ReplaceChar('-', '+');
  mSignature.ReplaceChar('_', '/');

  ScopedSECKEYPublicKey publicKey;
  ScopedSECItem signatureItem(::SECITEM_AllocItem(nullptr, nullptr, 0));
  SECOidTag oid;
  nsresult rv = ParseInput(publicKey, signatureItem, oid, locker);
  if (NS_FAILED(rv)) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  mCx = UniqueVFYContext(VFY_CreateContext(publicKey, signatureItem, oid, NULL));
  if (!mCx) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  if (VFY_Begin(mCx.get()) != SECSuccess) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // add the prefix to the verification buffer
  return Update(kPREFIX);
}

/**
 * Add data to the context that should be verified.
 */
nsresult
ContentVerifier::Update(const nsACString& aData)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  if (!aData.IsEmpty()) {
    if (VFY_Update(mCx.get(),
                   (const unsigned char*)nsPromiseFlatCString(aData).get(),
                   aData.Length()) != SECSuccess) {
      return NS_ERROR_INVALID_SIGNATURE;
    }
  }

  return NS_OK;
}

/**
 * Finish signature verification and return the result in _retval.
 */
nsresult
ContentVerifier::End(bool* _retval)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  *_retval = (VFY_End(mCx.get()) == SECSuccess);

  return NS_OK;
}