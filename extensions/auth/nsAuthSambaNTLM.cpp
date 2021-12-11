/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAuth.h"
#include "nsAuthSambaNTLM.h"
#include "nsMemory.h"
#include "nspr.h"
#include "prenv.h"
#include "plbase64.h"
#include "prerror.h"
#include "mozilla/Telemetry.h"

#include <stdlib.h>

nsAuthSambaNTLM::nsAuthSambaNTLM()
    : mInitialMessage(nullptr),
      mChildPID(nullptr),
      mFromChildFD(nullptr),
      mToChildFD(nullptr) {}

nsAuthSambaNTLM::~nsAuthSambaNTLM() {
  // ntlm_auth reads from stdin regularly so closing our file handles
  // should cause it to exit.
  Shutdown();
  PR_Free(mInitialMessage);
}

void nsAuthSambaNTLM::Shutdown() {
  if (mFromChildFD) {
    PR_Close(mFromChildFD);
    mFromChildFD = nullptr;
  }
  if (mToChildFD) {
    PR_Close(mToChildFD);
    mToChildFD = nullptr;
  }
  if (mChildPID) {
    PR_KillProcess(mChildPID);
    mChildPID = nullptr;
  }
}

NS_IMPL_ISUPPORTS(nsAuthSambaNTLM, nsIAuthModule)

static bool SpawnIOChild(char* const* aArgs, PRProcess** aPID,
                         PRFileDesc** aFromChildFD, PRFileDesc** aToChildFD) {
  PRFileDesc* toChildPipeRead;
  PRFileDesc* toChildPipeWrite;
  if (PR_CreatePipe(&toChildPipeRead, &toChildPipeWrite) != PR_SUCCESS) {
    return false;
  }
  PR_SetFDInheritable(toChildPipeRead, true);
  PR_SetFDInheritable(toChildPipeWrite, false);

  PRFileDesc* fromChildPipeRead;
  PRFileDesc* fromChildPipeWrite;
  if (PR_CreatePipe(&fromChildPipeRead, &fromChildPipeWrite) != PR_SUCCESS) {
    PR_Close(toChildPipeRead);
    PR_Close(toChildPipeWrite);
    return false;
  }
  PR_SetFDInheritable(fromChildPipeRead, false);
  PR_SetFDInheritable(fromChildPipeWrite, true);

  PRProcessAttr* attr = PR_NewProcessAttr();
  if (!attr) {
    PR_Close(fromChildPipeRead);
    PR_Close(fromChildPipeWrite);
    PR_Close(toChildPipeRead);
    PR_Close(toChildPipeWrite);
    return false;
  }

  PR_ProcessAttrSetStdioRedirect(attr, PR_StandardInput, toChildPipeRead);
  PR_ProcessAttrSetStdioRedirect(attr, PR_StandardOutput, fromChildPipeWrite);

  PRProcess* process = PR_CreateProcess(aArgs[0], aArgs, nullptr, attr);
  PR_DestroyProcessAttr(attr);
  PR_Close(fromChildPipeWrite);
  PR_Close(toChildPipeRead);
  if (!process) {
    LOG(("ntlm_auth exec failure [%d]", PR_GetError()));
    PR_Close(fromChildPipeRead);
    PR_Close(toChildPipeWrite);
    return false;
  }

  *aPID = process;
  *aFromChildFD = fromChildPipeRead;
  *aToChildFD = toChildPipeWrite;
  return true;
}

static bool WriteString(PRFileDesc* aFD, const nsACString& aString) {
  int32_t length = aString.Length();
  const char* s = aString.BeginReading();
  LOG(("Writing to ntlm_auth: %s", s));

  while (length > 0) {
    int result = PR_Write(aFD, s, length);
    if (result <= 0) return false;
    s += result;
    length -= result;
  }
  return true;
}

static bool ReadLine(PRFileDesc* aFD, nsACString& aString) {
  // ntlm_auth is defined to only send one line in response to each of our
  // input lines. So this simple unbuffered strategy works as long as we
  // read the response immediately after sending one request.
  aString.Truncate();
  for (;;) {
    char buf[1024];
    int result = PR_Read(aFD, buf, sizeof(buf));
    if (result <= 0) return false;
    aString.Append(buf, result);
    if (buf[result - 1] == '\n') {
      LOG(("Read from ntlm_auth: %s", nsPromiseFlatCString(aString).get()));
      return true;
    }
  }
}

/**
 * Returns a heap-allocated array of PRUint8s, and stores the length in aLen.
 * Returns nullptr if there's an error of any kind.
 */
static uint8_t* ExtractMessage(const nsACString& aLine, uint32_t* aLen) {
  // ntlm_auth sends blobs to us as base64-encoded strings after the "xx "
  // preamble on the response line.
  int32_t length = aLine.Length();
  // The caller should verify there is a valid "xx " prefix and the line
  // is terminated with a \n
  NS_ASSERTION(length >= 4, "Line too short...");
  const char* line = aLine.BeginReading();
  const char* s = line + 3;
  length -= 4;  // lose first 3 chars plus trailing \n
  NS_ASSERTION(s[length] == '\n', "aLine not newline-terminated");

  if (length & 3) {
    // The base64 encoded block must be multiple of 4. If not, something
    // screwed up.
    NS_WARNING("Base64 encoded block should be a multiple of 4 chars");
    return nullptr;
  }

  // Calculate the exact length. I wonder why there isn't a function for this
  // in plbase64.
  int32_t numEquals;
  for (numEquals = 0; numEquals < length; ++numEquals) {
    if (s[length - 1 - numEquals] != '=') break;
  }
  *aLen = (length / 4) * 3 - numEquals;
  return reinterpret_cast<uint8_t*>(PL_Base64Decode(s, length, nullptr));
}

nsresult nsAuthSambaNTLM::SpawnNTLMAuthHelper() {
  const char* username = PR_GetEnv("USER");
  if (!username) return NS_ERROR_FAILURE;

  const char* const args[] = {"ntlm_auth",
                              "--helper-protocol",
                              "ntlmssp-client-1",
                              "--use-cached-creds",
                              "--username",
                              username,
                              nullptr};

  bool isOK = SpawnIOChild(const_cast<char* const*>(args), &mChildPID,
                           &mFromChildFD, &mToChildFD);
  if (!isOK) return NS_ERROR_FAILURE;

  if (!WriteString(mToChildFD, "YR\n"_ns)) return NS_ERROR_FAILURE;
  nsCString line;
  if (!ReadLine(mFromChildFD, line)) return NS_ERROR_FAILURE;
  if (!StringBeginsWith(line, "YR "_ns)) {
    // Something went wrong. Perhaps no credentials are accessible.
    return NS_ERROR_FAILURE;
  }

  // It gave us an initial client-to-server request packet. Save that
  // because we'll need it later.
  mInitialMessage = ExtractMessage(line, &mInitialMessageLen);
  if (!mInitialMessage) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Init(const nsACString& serviceName, uint32_t serviceFlags,
                      const nsAString& domain, const nsAString& username,
                      const nsAString& password) {
  NS_ASSERTION(username.IsEmpty() && domain.IsEmpty() && password.IsEmpty(),
               "unexpected credentials");

  static bool sTelemetrySent = false;
  if (!sTelemetrySent) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::NTLM_MODULE_USED_2,
                                   serviceFlags & nsIAuthModule::REQ_PROXY_AUTH
                                       ? NTLM_MODULE_SAMBA_AUTH_PROXY
                                       : NTLM_MODULE_SAMBA_AUTH_DIRECT);
    sTelemetrySent = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAuthSambaNTLM::GetNextToken(const void* inToken, uint32_t inTokenLen,
                              void** outToken, uint32_t* outTokenLen) {
  if (!inToken) {
    /* someone wants our initial message */
    *outToken = moz_xmemdup(mInitialMessage, mInitialMessageLen);
    *outTokenLen = mInitialMessageLen;
    return NS_OK;
  }

  /* inToken must be a type 2 message. Get ntlm_auth to generate our response */
  char* encoded =
      PL_Base64Encode(static_cast<const char*>(inToken), inTokenLen, nullptr);
  if (!encoded) return NS_ERROR_OUT_OF_MEMORY;

  nsCString request;
  request.AssignLiteral("TT ");
  request.Append(encoded);
  PR_Free(encoded);
  request.Append('\n');

  if (!WriteString(mToChildFD, request)) return NS_ERROR_FAILURE;
  nsCString line;
  if (!ReadLine(mFromChildFD, line)) return NS_ERROR_FAILURE;
  if (!StringBeginsWith(line, "KK "_ns) && !StringBeginsWith(line, "AF "_ns)) {
    // Something went wrong. Perhaps no credentials are accessible.
    return NS_ERROR_FAILURE;
  }
  uint8_t* buf = ExtractMessage(line, outTokenLen);
  if (!buf) return NS_ERROR_FAILURE;
  *outToken = moz_xmemdup(buf, *outTokenLen);
  PR_Free(buf);

  // We're done. Close our file descriptors now and reap the helper
  // process.
  Shutdown();
  return NS_SUCCESS_AUTH_FINISHED;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Unwrap(const void* inToken, uint32_t inTokenLen,
                        void** outToken, uint32_t* outTokenLen) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Wrap(const void* inToken, uint32_t inTokenLen,
                      bool confidential, void** outToken,
                      uint32_t* outTokenLen) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
