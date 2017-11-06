/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBroker.h"
#include "FunctionBrokerParent.h"
#include "PluginQuirks.h"

#if defined(XP_WIN)
#include <commdlg.h>
#include <schannel.h>
#endif // defined(XP_WIN)

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::plugins;

namespace mozilla {
namespace plugins {

template <int QuirkFlag>
static bool CheckQuirks(int aQuirks)
{
  return static_cast<bool>(aQuirks & QuirkFlag);
}

void FreeDestructor(void* aObj) { free(aObj); }

#if defined(XP_WIN)

/* GetKeyState */

typedef FunctionBroker<ID_GetKeyState,
                       decltype(GetKeyState)> GetKeyStateFB;

template<>
ShouldHookFunc* const
GetKeyStateFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_GETKEYSTATE>;

/* SetCursorPos */

typedef FunctionBroker<ID_SetCursorPos,
                       decltype(SetCursorPos)> SetCursorPosFB;

/* GetSaveFileNameW */

typedef FunctionBroker<ID_GetSaveFileNameW,
                             decltype(GetSaveFileNameW)> GetSaveFileNameWFB;

// Remember files granted access in the chrome process
static void GrantFileAccess(base::ProcessId aClientId, LPOPENFILENAME& aLpofn,
                            bool isSave)
{
#if defined(MOZ_SANDBOX)
  if (aLpofn->Flags & OFN_ALLOWMULTISELECT) {
    // We only support multiselect with the OFN_EXPLORER flag.
    // This guarantees that ofn.lpstrFile follows the pattern below.
    MOZ_ASSERT(aLpofn->Flags & OFN_EXPLORER);

    // lpstrFile is one of two things:
    // 1. A null terminated full path to a file, or
    // 2. A path to a folder, followed by a NULL, followed by a
    // list of file names, each NULL terminated, followed by an
    // additional NULL (so it is also double-NULL terminated).
    std::wstring path = std::wstring(aLpofn->lpstrFile);
    MOZ_ASSERT(aLpofn->nFileOffset > 0);
    // For condition #1, nFileOffset points to the file name in the path.
    // It will be preceeded by a non-NULL character from the path.
    if (aLpofn->lpstrFile[aLpofn->nFileOffset-1] != L'\0') {
      FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, path.c_str(), isSave);
    }
    else {
      // This is condition #2
      wchar_t* nextFile = aLpofn->lpstrFile + path.size() + 1;
      while (*nextFile != L'\0') {
        std::wstring nextFileStr(nextFile);
        std::wstring fullPath =
          path + std::wstring(L"\\") + nextFileStr;
        FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, fullPath.c_str(), isSave);
        nextFile += nextFileStr.size() + 1;
      }
    }
  }
  else {
    FunctionBrokerParent::GetSandboxPermissions()->GrantFileAccess(aClientId, aLpofn->lpstrFile, isSave);
  }
#else
  MOZ_ASSERT_UNREACHABLE("GetFileName IPC message is only available on "
                         "Windows builds with sandbox.");
#endif
}

template<> template<>
BOOL GetSaveFileNameWFB::RunFunction(GetSaveFileNameWFB::FunctionType* aOrigFunction,
                                    base::ProcessId aClientId,
                                    LPOPENFILENAMEW& aLpofn) const
{
  BOOL result = aOrigFunction(aLpofn);
  if (result) {
    // Record any file access permission that was just granted.
    GrantFileAccess(aClientId, aLpofn, true);
  }
  return result;
}

template<> template<>
struct GetSaveFileNameWFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };

/* GetOpenFileNameW */

typedef FunctionBroker<ID_GetOpenFileNameW,
                       decltype(GetOpenFileNameW)> GetOpenFileNameWFB;

template<> template<>
BOOL GetOpenFileNameWFB::RunFunction(GetOpenFileNameWFB::FunctionType* aOrigFunction,
                                    base::ProcessId aClientId,
                                    LPOPENFILENAMEW& aLpofn) const
{
  BOOL result = aOrigFunction(aLpofn);
  if (result) {
    // Record any file access permission that was just granted.
    GrantFileAccess(aClientId, aLpofn, false);
  }
  return result;
}

template<> template<>
struct GetOpenFileNameWFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };

/* InternetOpenA */

typedef FunctionBroker<ID_InternetOpenA,
                       decltype(InternetOpenA)> InternetOpenAFB;

/* InternetConnectA */

typedef FunctionBroker<ID_InternetConnectA,
                             decltype(InternetConnectA)> InternetConnectAFB;

typedef InternetConnectAFB::Request ICAReqHandler;

template<>
bool ICAReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                  const LPCSTR& srv, const INTERNET_PORT& port,
                                  const LPCSTR& user, const LPCSTR& pass,
                                  const DWORD& svc, const DWORD& flags,
                                  const DWORD_PTR& cxt)
{
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* InternetCloseHandle */

typedef FunctionBroker<ID_InternetCloseHandle,
                       decltype(InternetCloseHandle)> InternetCloseHandleFB;

typedef InternetCloseHandleFB::Request ICHReqHandler;

template<>
bool ICHReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h)
{
  // If we are server side then we were already validated since we had to be
  // looked up in the "uint64_t <-> HINTERNET" hashtable.
  // In the client, we check that this is a dummy handle.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* InternetQueryDataAvailable */

typedef FunctionBroker<ID_InternetQueryDataAvailable,
                       decltype(InternetQueryDataAvailable)> InternetQueryDataAvailableFB;

typedef InternetQueryDataAvailableFB::Request IQDAReq;

typedef struct RequestHandler<ID_InternetQueryDataAvailable,
                              BOOL HOOK_CALL (HINTERNET)> IQDADelegateReq;

template<>
void IQDAReq::Marshal(IpdlTuple& aTuple, const HINTERNET& file,
                      const LPDWORD& nBytes, const DWORD& flags,
                      const DWORD_PTR& cxt)
{
  IQDADelegateReq::Marshal(aTuple, file);
}

template<>
bool IQDAReq::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& file,
                        LPDWORD& nBytes, DWORD& flags, DWORD_PTR& cxt)
{
  bool success = IQDADelegateReq::Unmarshal(aScd, aTuple, file);
  if (!success) {
    return false;
  }
  flags = 0;
  cxt = 0;
  nBytes = aScd.Allocate<DWORD>();
  return true;
}

template<>
bool IQDAReq::ShouldBroker(Endpoint endpoint, const HINTERNET& file,
                           const LPDWORD& nBytes, const DWORD& flags,
                           const DWORD_PTR& cxt)
{
  // If we are server side then we were already validated since we had to be
  // looked up in the "uint64_t <-> HINTERNET" hashtable.
  // In the client, we check that this is a dummy handle.
  return (endpoint == SERVER) ||
         ((flags == 0) && (cxt == 0) &&
          IsOdd(reinterpret_cast<uint64_t>(file)));
}

template<> template<>
struct InternetQueryDataAvailableFB::Response::Info::ShouldMarshal<1> { static const bool value = true; };

/* InternetReadFile */

typedef FunctionBroker<ID_InternetReadFile,
                       decltype(InternetReadFile)> InternetReadFileFB;

typedef InternetReadFileFB::Request IRFRequestHandler;
typedef struct RequestHandler<ID_InternetReadFile,
                              BOOL HOOK_CALL (HINTERNET, DWORD)> IRFDelegateReq;

template<>
void IRFRequestHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h,
                                const LPVOID& buf, const DWORD& nBytesToRead,
                                const LPDWORD& nBytesRead)
{
  IRFDelegateReq::Marshal(aTuple, h, nBytesToRead);
}


template<>
bool IRFRequestHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                                  LPVOID& buf, DWORD& nBytesToRead,
                                  LPDWORD& nBytesRead)
{
  bool ret = IRFDelegateReq::Unmarshal(aScd, aTuple, h, nBytesToRead);
  if (!ret) {
    return false;
  }

  nBytesRead = aScd.Allocate<DWORD>();
  MOZ_ASSERT(nBytesToRead > 0);
  aScd.AllocateMemory(nBytesToRead, buf);
  return true;
}

template<>
bool IRFRequestHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                     const LPVOID& buf, const DWORD& nBytesToRead,
                                     const LPDWORD& nBytesRead)
{
  // For server-side validation, the HINTERNET deserialization will have
  // required it to already be looked up in the IdToPtrMap.  At that point,
  // any call is valid.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

// Marshal the output parameter that we sent to the response delegate.
template<> template<>
struct InternetReadFileFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };

typedef ResponseHandler<ID_InternetReadFile, decltype(InternetReadFile)> IRFResponseHandler;
typedef ResponseHandler<ID_InternetReadFile,
                               BOOL HOOK_CALL (nsDependentCSubstring)> IRFDelegateResponseHandler;

template<>
void IRFResponseHandler::Marshal(IpdlTuple& aTuple, const BOOL& ret, const HINTERNET& h,
                                 const LPVOID& buf, const DWORD& nBytesToRead,
                                 const LPDWORD& nBytesRead)
{
  nsDependentCSubstring str;
  if (*nBytesRead) {
    str.Assign(static_cast<const char*>(buf), *nBytesRead);
  }
  IRFDelegateResponseHandler::Marshal(aTuple, ret, str);
}

template<>
bool IRFResponseHandler::Unmarshal(const IpdlTuple& aTuple, BOOL& ret, HINTERNET& h,
                                   LPVOID& buf, DWORD& nBytesToRead,
                                   LPDWORD& nBytesRead)
{
  nsDependentCSubstring str;
  bool success =
    IRFDelegateResponseHandler::Unmarshal(aTuple, ret, str);
  if (!success) {
    return false;
  }

  if (str.Length()) {
    memcpy(buf, str.Data(), str.Length());
    *nBytesRead = str.Length();
  }
  return true;
}

/* InternetWriteFile */

typedef FunctionBroker<ID_InternetWriteFile,
                       decltype(InternetWriteFile)> InternetWriteFileFB;

typedef InternetWriteFileFB::Request IWFReqHandler;
typedef RequestHandler<ID_InternetWriteFile,
                       int HOOK_CALL (HINTERNET, nsDependentCSubstring)> IWFDelegateReqHandler;

template<>
void IWFReqHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& file, const LPCVOID& buf,
                            const DWORD& nToWrite, const LPDWORD& nWritten)
{
  MOZ_ASSERT(nWritten);
  IWFDelegateReqHandler::Marshal(aTuple, file,
                                  nsDependentCSubstring(static_cast<const char*>(buf), nToWrite));
}

template<>
bool IWFReqHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& file,
                              LPCVOID& buf, DWORD& nToWrite, LPDWORD& nWritten)
{
  nsDependentCSubstring str;
  if (!IWFDelegateReqHandler::Unmarshal(aScd, aTuple, file, str)) {
    return false;
  }

  aScd.AllocateString(str, buf, false);
  nToWrite = str.Length();
  nWritten = aScd.Allocate<DWORD>();
  return true;
}

template<>
bool IWFReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& file,
                                 const LPCVOID& buf, const DWORD& nToWrite,
                                 const LPDWORD& nWritten)
{
  // For server-side validation, the HINTERNET deserialization will have
  // required it to already be looked up in the IdToPtrMap.  At that point,
  // any call is valid.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(file));
}

template<> template<>
struct InternetWriteFileFB::Response::Info::ShouldMarshal<3> { static const bool value = true; };

/* InternetSetOptionA */

typedef FunctionBroker<ID_InternetSetOptionA,
                       decltype(InternetSetOptionA)> InternetSetOptionAFB;

typedef InternetSetOptionAFB::Request ISOAReqHandler;
typedef RequestHandler<ID_InternetSetOptionA,
                       BOOL HOOK_CALL (HINTERNET, DWORD, nsDependentCSubstring)> ISOADelegateReqHandler;

template<>
void ISOAReqHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h, const DWORD& opt,
                             const LPVOID& buf, const DWORD& bufLen)
{
  ISOADelegateReqHandler::Marshal(aTuple, h, opt,
                                  nsDependentCSubstring(static_cast<const char*>(buf), bufLen));
}

template<>
bool ISOAReqHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                               DWORD& opt, LPVOID& buf, DWORD& bufLen)
{
  nsDependentCSubstring str;
  if (!ISOADelegateReqHandler::Unmarshal(aScd, aTuple, h, opt, str)) {
    return false;
  }

  aScd.AllocateString(str, buf, false);
  bufLen = str.Length();
  return true;
}

template<>
bool ISOAReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h, const DWORD& opt,
                                  const LPVOID& buf, const DWORD& bufLen)
{
  // For server-side validation, the HINTERNET deserialization will have
  // required it to already be looked up in the IdToPtrMap.  At that point,
  // any call is valid.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* HttpAddRequestHeadersA */

typedef FunctionBroker<ID_HttpAddRequestHeadersA,
                       decltype(HttpAddRequestHeadersA)> HttpAddRequestHeadersAFB;

typedef HttpAddRequestHeadersAFB::Request HARHAReqHandler;

template<>
bool HARHAReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                   const LPCSTR& head, const DWORD& headLen,
                                   const DWORD& mods)
{
  // For server-side validation, the HINTERNET deserialization will have
  // required it to already be looked up in the IdToPtrMap.  At that point,
  // any call is valid.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* HttpOpenRequestA */

typedef FunctionBroker<ID_HttpOpenRequestA,
                       decltype(HttpOpenRequestA)> HttpOpenRequestAFB;

typedef HttpOpenRequestAFB::Request HORAReqHandler;
typedef RequestHandler<ID_HttpOpenRequestA,
                       HINTERNET HOOK_CALL (HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR,
                                            nsTArray<nsCString>, DWORD, DWORD_PTR)> HORADelegateReqHandler;

template<>
void HORAReqHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h,
                             const LPCSTR& verb, const LPCSTR& obj,
                             const LPCSTR& ver, const LPCSTR& ref,
                             LPCSTR * const & acceptTypes, const DWORD& flags,
                             const DWORD_PTR& cxt)
{
  nsTArray<nsCString> arrayAcceptTypes;
  LPCSTR * curAcceptType = acceptTypes;
  if (curAcceptType) {
    while (*curAcceptType) {
      arrayAcceptTypes.AppendElement(nsCString(*curAcceptType));
      ++curAcceptType;
    }
  }
  HORADelegateReqHandler::Marshal(aTuple, h, verb, obj, ver, ref,
                                  arrayAcceptTypes, flags, cxt);
}

template<>
bool HORAReqHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                               LPCSTR& verb, LPCSTR& obj, LPCSTR& ver,
                               LPCSTR& ref, LPCSTR *& acceptTypes,
                               DWORD& flags, DWORD_PTR& cxt)
{
  nsTArray<nsCString> arrayAcceptTypes;
  if (!HORADelegateReqHandler::Unmarshal(aScd, aTuple, h, verb, obj, ver, ref, arrayAcceptTypes, flags, cxt)) {
    return false;
  }
  if (arrayAcceptTypes.Length() == 0) {
    acceptTypes = nullptr;
  } else {
    aScd.AllocateMemory((arrayAcceptTypes.Length() + 1) * sizeof(LPCSTR), acceptTypes);
    for (size_t i = 0; i < arrayAcceptTypes.Length(); ++i) {
      aScd.AllocateString(arrayAcceptTypes[i], acceptTypes[i]);
    }
    acceptTypes[arrayAcceptTypes.Length()] = nullptr;
  }
  return true;
}

template<>
bool HORAReqHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                  const LPCSTR& verb, const LPCSTR& obj,
                                  const LPCSTR& ver, const LPCSTR& ref,
                                  LPCSTR * const & acceptTypes,
                                  const DWORD& flags, const DWORD_PTR& cxt)
{
  // For the server-side test, the HINTERNET deserialization will have
  // required it to already be looked up in the IdToPtrMap.  At that point,
  // any call is valid.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* HttpQueryInfoA */

typedef FunctionBroker<ID_HttpQueryInfoA,
                       decltype(HttpQueryInfoA)> HttpQueryInfoAFB;

typedef HttpQueryInfoAFB::Request HQIARequestHandler;
typedef RequestHandler<ID_HttpQueryInfoA,
                       BOOL HOOK_CALL (HINTERNET, DWORD, BOOL, DWORD, BOOL,
                                       DWORD)> HQIADelegateRequestHandler;

template<>
void HQIARequestHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h,
                                 const DWORD& lvl, const LPVOID& buf, const LPDWORD& bufLen,
                                 const LPDWORD& idx)
{
  HQIADelegateRequestHandler::Marshal(aTuple, h, lvl,
                                      bufLen != nullptr, bufLen ? *bufLen : 0,
                                      idx != nullptr, idx ? *idx : 0);
}

template<>
bool HQIARequestHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                                   DWORD& lvl, LPVOID& buf, LPDWORD& bufLen,
                                   LPDWORD& idx)
{
  BOOL hasBufLen, hasIdx;
  DWORD tempBufLen, tempIdx;
  bool success =
    HQIADelegateRequestHandler::Unmarshal(aScd, aTuple, h, lvl, hasBufLen, tempBufLen, hasIdx, tempIdx);
  if (!success) {
    return false;
  }

  bufLen = nullptr;
  if (hasBufLen) {
    aScd.AllocateMemory(tempBufLen, buf, bufLen);
  }

  idx = nullptr;
  if (hasIdx) {
    idx = aScd.Allocate<DWORD>(tempIdx);
  }

  return true;
}

template<>
bool HQIARequestHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                      const DWORD& lvl, const LPVOID& buf,
                                      const LPDWORD& bufLen, const LPDWORD& idx)
{
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

// Marshal all of the output parameters that we sent to the response delegate.
template<> template<>
struct HttpQueryInfoAFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };
template<> template<>
struct HttpQueryInfoAFB::Response::Info::ShouldMarshal<1> { static const bool value = true; };
template<> template<>
struct HttpQueryInfoAFB::Response::Info::ShouldMarshal<2> { static const bool value = true; };

typedef HttpQueryInfoAFB::Response HQIAResponseHandler;
typedef ResponseHandler<ID_HttpQueryInfoA,
                        BOOL HOOK_CALL (nsDependentCSubstring,
                                        DWORD, DWORD)> HQIADelegateResponseHandler;

template<>
void HQIAResponseHandler::Marshal(IpdlTuple& aTuple, const BOOL& ret, const HINTERNET& h,
                                 const DWORD& lvl, const LPVOID& buf, const LPDWORD& bufLen,
                                 const LPDWORD& idx)
{
  nsDependentCSubstring str;
  if (buf && ret) {
    MOZ_ASSERT(bufLen);
    str.Assign(static_cast<const char*>(buf), *bufLen);
  }
  // Note that we send the bufLen separately to handle the case where buf wasn't
  // allocated or large enough to hold the entire return value.  bufLen is then
  // the required buffer size.
  HQIADelegateResponseHandler::Marshal(aTuple, ret, str,
                                       bufLen ? *bufLen : 0, idx ? *idx : 0);
}

template<>
bool HQIAResponseHandler::Unmarshal(const IpdlTuple& aTuple, BOOL& ret, HINTERNET& h,
                                   DWORD& lvl, LPVOID& buf, LPDWORD& bufLen,
                                   LPDWORD& idx)
{
  DWORD totalBufLen = *bufLen;
  nsDependentCSubstring str;
  DWORD tempBufLen, tempIdx;
  bool success =
    HQIADelegateResponseHandler::Unmarshal(aTuple, ret, str, tempBufLen, tempIdx);
  if (!success) {
    return false;
  }

  if (bufLen) {
    *bufLen = tempBufLen;
  }
  if (idx) {
    *idx = tempIdx;
  }

  if (buf && ret) {
    // When HttpQueryInfo returns strings, the buffer length will not include
    // the null terminator.  Rather than (brittle-y) trying to determine if the
    // return buffer is a string, we always tack on a null terminator if the
    // buffer has room for it.
    MOZ_ASSERT(str.Length() == *bufLen);
    memcpy(buf, str.Data(), str.Length());
    if (str.Length() < totalBufLen) {
      char* cbuf = static_cast<char*>(buf);
      cbuf[str.Length()] = '\0';
    }
  }
  return true;
}

/* HttpSendRequestA */

typedef FunctionBroker<ID_HttpSendRequestA,
                       decltype(HttpSendRequestA)> HttpSendRequestAFB;

typedef HttpSendRequestAFB::Request HSRARequestHandler;
typedef RequestHandler<ID_HttpSendRequestA,
                       BOOL HOOK_CALL (HINTERNET, nsDependentCSubstring,
                                       nsDependentCSubstring)> HSRADelegateRequestHandler;

template<>
void HSRARequestHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h,
                                 const LPCSTR& head, const DWORD& headLen,
                                 const LPVOID& opt, const DWORD& optLen)
{
  nsDependentCSubstring headStr;
  headStr.SetIsVoid(head == nullptr);
  if (head) {
    // HttpSendRequest allows headLen == -1L for length of a null terminated string.
    DWORD ncHeadLen = headLen;
    if (ncHeadLen == -1L) {
      ncHeadLen = strlen(head);
    }
    headStr.Rebind(head, ncHeadLen);
  }
  nsDependentCSubstring optStr;
  optStr.SetIsVoid(opt == nullptr);
  if (opt) {
    optStr.Rebind(static_cast<const char*>(opt), optLen);
  }
  HSRADelegateRequestHandler::Marshal(aTuple, h, headStr, optStr);
}

template<>
bool HSRARequestHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                                   LPCSTR& head, DWORD& headLen, LPVOID& opt,
                                   DWORD& optLen)
{
  nsDependentCSubstring headStr;
  nsDependentCSubstring optStr;
  bool success = HSRADelegateRequestHandler::Unmarshal(aScd, aTuple, h, headStr, optStr);
  if (!success) {
    return false;
  }

  if (headStr.IsVoid()) {
    head = nullptr;
    MOZ_ASSERT(headLen == 0);
  } else {
    aScd.AllocateString(headStr, head, false);
    headLen = headStr.Length();
  }

  if (optStr.IsVoid()) {
    opt = nullptr;
    MOZ_ASSERT(optLen == 0);
  } else {
    aScd.AllocateString(optStr, opt, false);
    optLen = optStr.Length();
  }
  return true;
}

template<>
bool HSRARequestHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                       const LPCSTR& head, const DWORD& headLen,
                                       const LPVOID& opt, const DWORD& optLen)
{
  // If we are server side then we were already validated since we had to be
  // looked up in the "uint64_t <-> HINTERNET" hashtable.
  // In the client, we check that this is a dummy handle.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

/* HttpSendRequestExA */

typedef FunctionBroker<ID_HttpSendRequestExA,
                       decltype(HttpSendRequestExA)> HttpSendRequestExAFB;

typedef RequestInfo<ID_HttpSendRequestExA> HSRExAReqInfo;

template<> template<>
struct HSRExAReqInfo::FixedValue<2> { static const LPINTERNET_BUFFERSA value; };
const LPINTERNET_BUFFERSA HSRExAReqInfo::FixedValue<2>::value = nullptr;

// Docs for HttpSendRequestExA say this parameter 'must' be zero but Flash
// passes other values.
// template<> template<>
// struct HSRExAReqInfo::FixedValue<3> { static const DWORD value = 0; };

template<> template<>
struct HSRExAReqInfo::FixedValue<4> { static const DWORD_PTR value = 0; };

/* InternetQueryOptionA */

typedef FunctionBroker<ID_InternetQueryOptionA,
                       decltype(InternetQueryOptionA)> InternetQueryOptionAFB;

typedef InternetQueryOptionAFB::Request IQOARequestHandler;
typedef RequestHandler<ID_InternetQueryOptionA,
                       BOOL HOOK_CALL (HINTERNET, DWORD, DWORD)> IQOADelegateRequestHandler;

template<>
void IQOARequestHandler::Marshal(IpdlTuple& aTuple, const HINTERNET& h,
                                 const DWORD& opt, const LPVOID& buf, const LPDWORD& bufLen)
{
  MOZ_ASSERT(bufLen);
  IQOADelegateRequestHandler::Marshal(aTuple, h, opt, buf ? *bufLen : 0);
}

template<>
bool IQOARequestHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, HINTERNET& h,
                                   DWORD& opt, LPVOID& buf, LPDWORD& bufLen)
{
  DWORD tempBufLen;
  bool success =
    IQOADelegateRequestHandler::Unmarshal(aScd, aTuple, h, opt, tempBufLen);
  if (!success) {
    return false;
  }

  aScd.AllocateMemory(tempBufLen, buf, bufLen);
  return true;
}

template<>
bool IQOARequestHandler::ShouldBroker(Endpoint endpoint, const HINTERNET& h,
                                       const DWORD& opt, const LPVOID& buf,
                                       const LPDWORD& bufLen)
{
  // If we are server side then we were already validated since we had to be
  // looked up in the "uint64_t <-> HINTERNET" hashtable.
  // In the client, we check that this is a dummy handle.
  return (endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h));
}

// Marshal all of the output parameters that we sent to the response delegate.
template<> template<>
struct InternetQueryOptionAFB::Response::Info::ShouldMarshal<0> { static const bool value = true; };
template<> template<>
struct InternetQueryOptionAFB::Response::Info::ShouldMarshal<1> { static const bool value = true; };

typedef InternetQueryOptionAFB::Response IQOAResponseHandler;
typedef ResponseHandler<ID_InternetQueryOptionA,
                        BOOL HOOK_CALL (nsDependentCSubstring, DWORD)> IQOADelegateResponseHandler;

template<>
void IQOAResponseHandler::Marshal(IpdlTuple& aTuple, const BOOL& ret, const HINTERNET& h,
                                  const DWORD& opt, const LPVOID& buf, const LPDWORD& bufLen)
{
  nsDependentCSubstring str;
  if (buf && ret) {
    MOZ_ASSERT(*bufLen);
    str.Assign(static_cast<const char*>(buf), *bufLen);
  }
  IQOADelegateResponseHandler::Marshal(aTuple, ret, str, *bufLen);
}

template<>
bool IQOAResponseHandler::Unmarshal(const IpdlTuple& aTuple, BOOL& ret, HINTERNET& h,
                                   DWORD& opt, LPVOID& buf, LPDWORD& bufLen)
{
  nsDependentCSubstring str;
  bool success =
    IQOADelegateResponseHandler::Unmarshal(aTuple, ret, str, *bufLen);
  if (!success) {
    return false;
  }

  if (buf && ret) {
    MOZ_ASSERT(str.Length() == *bufLen);
    memcpy(buf, str.Data(), str.Length());
  }
  return true;
}

/* InternetErrorDlg */

typedef FunctionBroker<ID_InternetErrorDlg,
                       decltype(InternetErrorDlg)> InternetErrorDlgFB;

typedef RequestInfo<ID_InternetErrorDlg> IEDReqInfo;

template<> template<>
struct IEDReqInfo::FixedValue<4> { static LPVOID* const value; };
LPVOID* const IEDReqInfo::FixedValue<4>::value = nullptr;

typedef InternetErrorDlgFB::Request IEDReqHandler;

template<>
bool IEDReqHandler::ShouldBroker(Endpoint endpoint, const HWND& hwnd,
                                 const HINTERNET& h, const DWORD& err,
                                 const DWORD& flags, LPVOID* const & data)
{
  const DWORD SUPPORTED_FLAGS =
    FLAGS_ERROR_UI_FILTER_FOR_ERRORS | FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
    FLAGS_ERROR_UI_FLAGS_GENERATE_DATA | FLAGS_ERROR_UI_FLAGS_NO_UI;

  // We broker if (1) the handle h is brokered (odd in client),
  // (2) we support the requested action flags and (3) there is no user
  // data, which wouldn't make sense for our supported flags anyway.
  return ((endpoint == SERVER) || IsOdd(reinterpret_cast<uint64_t>(h))) &&
         (!(flags & ~SUPPORTED_FLAGS)) && (data == nullptr);
}

/* AcquireCredentialsHandleA */

typedef FunctionBroker<ID_AcquireCredentialsHandleA,
                       decltype(AcquireCredentialsHandleA)> AcquireCredentialsHandleAFB;

typedef RequestInfo<ID_AcquireCredentialsHandleA> ACHAReqInfo;

template<> template<>
struct ACHAReqInfo::FixedValue<0> { static const LPSTR value; };
const LPSTR ACHAReqInfo::FixedValue<0>::value = nullptr;

template<> template<>
struct ACHAReqInfo::FixedValue<1> { static const LPSTR value; };
const LPSTR ACHAReqInfo::FixedValue<1>::value = UNISP_NAME_A;

template<> template<>
struct ACHAReqInfo::FixedValue<2> { static const unsigned long value; };
const unsigned long ACHAReqInfo::FixedValue<2>::value = SECPKG_CRED_OUTBOUND;

template<> template<>
struct ACHAReqInfo::FixedValue<3> { static void* const value; };
void* const ACHAReqInfo::FixedValue<3>::value = nullptr;

template<> template<>
struct ACHAReqInfo::FixedValue<5> { static const SEC_GET_KEY_FN value; };
const SEC_GET_KEY_FN ACHAReqInfo::FixedValue<5>::value = nullptr;

template<> template<>
struct ACHAReqInfo::FixedValue<6> { static void* const value; };
void* const ACHAReqInfo::FixedValue<6>::value = nullptr;

typedef AcquireCredentialsHandleAFB::Request ACHARequestHandler;
typedef RequestHandler<ID_AcquireCredentialsHandleA,
                       SECURITY_STATUS HOOK_CALL (LPSTR, LPSTR, unsigned long,
                                       void*, PSCHANNEL_CRED, SEC_GET_KEY_FN,
                                       void*)> ACHADelegateRequestHandler;

template<>
void ACHARequestHandler::Marshal(IpdlTuple& aTuple, const LPSTR& principal,
                                 const LPSTR& pkg, const unsigned long& credUse,
                                 const PVOID& logonId, const PVOID& auth,
                                 const SEC_GET_KEY_FN& getKeyFn,
                                 const PVOID& getKeyArg, const PCredHandle& cred,
                                 const PTimeStamp& expiry)
{
  const PSCHANNEL_CRED& scCred = reinterpret_cast<const PSCHANNEL_CRED&>(auth);
  ACHADelegateRequestHandler::Marshal(aTuple, principal, pkg, credUse, logonId,
                                      scCred, getKeyFn, getKeyArg);
}

template<>
bool ACHARequestHandler::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, LPSTR& principal,
                                   LPSTR& pkg, unsigned long& credUse,
                                   PVOID& logonId, PVOID& auth, SEC_GET_KEY_FN& getKeyFn,
                                   PVOID& getKeyArg, PCredHandle& cred, PTimeStamp& expiry)
{
  PSCHANNEL_CRED& scCred = reinterpret_cast<PSCHANNEL_CRED&>(auth);
  if (!ACHADelegateRequestHandler::Unmarshal(aScd, aTuple, principal, pkg, credUse,
                                             logonId, scCred, getKeyFn, getKeyArg)) {
    return false;
  }

  cred = aScd.Allocate<CredHandle>();
  expiry = aScd.Allocate<::TimeStamp>();
  return true;
}

typedef ResponseInfo<ID_AcquireCredentialsHandleA> ACHARspInfo;

// Response phase must send output parameters
template<> template<>
struct ACHARspInfo::ShouldMarshal<7> { static const bool value = true; };
template<> template<>
struct ACHARspInfo::ShouldMarshal<8> { static const bool value = true; };

/* QueryCredentialsAttributesA */

typedef FunctionBroker<ID_QueryCredentialsAttributesA,
                       decltype(QueryCredentialsAttributesA)> QueryCredentialsAttributesAFB;

/* FreeCredentialsHandle */

typedef FunctionBroker<ID_FreeCredentialsHandle,
                       decltype(FreeCredentialsHandle)> FreeCredentialsHandleFB;

typedef FreeCredentialsHandleFB::Request FCHReq;

template<>
bool FCHReq::ShouldBroker(Endpoint endpoint, const PCredHandle& h)
{
  // If we are server side then we were already validated since we had to be
  // looked up in the "uint64_t <-> CredHandle" hashtable.
  // In the client, we check that this is a dummy handle.
  return (endpoint == SERVER) ||
         ((h->dwLower == h->dwUpper) && IsOdd(static_cast<uint64_t>(h->dwLower)));
}

#endif // defined(XP_WIN)

/*****************************************************************************/

#define FUN_HOOK(x) static_cast<FunctionHook*>(x)
void
AddBrokeredFunctionHooks(FunctionHookArray& aHooks)
{
  // We transfer ownership of the FunctionHook objects to the array.
#if defined(XP_WIN)
  aHooks[ID_GetKeyState] =
    FUN_HOOK(new GetKeyStateFB("user32.dll", "GetKeyState", &GetKeyState));
  aHooks[ID_SetCursorPos] =
    FUN_HOOK(new SetCursorPosFB("user32.dll", "SetCursorPos", &SetCursorPos));
  aHooks[ID_GetSaveFileNameW] =
    FUN_HOOK(new GetSaveFileNameWFB("comdlg32.dll", "GetSaveFileNameW",
                                    &GetSaveFileNameW));
  aHooks[ID_GetOpenFileNameW] =
    FUN_HOOK(new GetOpenFileNameWFB("comdlg32.dll", "GetOpenFileNameW",
                                    &GetOpenFileNameW));
  aHooks[ID_InternetOpenA] =
    FUN_HOOK(new InternetOpenAFB("wininet.dll", "InternetOpenA", &InternetOpenA));
  aHooks[ID_InternetConnectA] =
    FUN_HOOK(new InternetConnectAFB("wininet.dll", "InternetConnectA",
                                    &InternetConnectA));
  aHooks[ID_InternetCloseHandle] =
    FUN_HOOK(new InternetCloseHandleFB("wininet.dll", "InternetCloseHandle",
                                       &InternetCloseHandle));
  aHooks[ID_InternetQueryDataAvailable] =
    FUN_HOOK(new InternetQueryDataAvailableFB("wininet.dll",
                                              "InternetQueryDataAvailable",
                                              &InternetQueryDataAvailable));
  aHooks[ID_InternetReadFile] =
    FUN_HOOK(new InternetReadFileFB("wininet.dll", "InternetReadFile",
                                    &InternetReadFile));
  aHooks[ID_InternetWriteFile] =
    FUN_HOOK(new InternetWriteFileFB("wininet.dll", "InternetWriteFile",
                                     &InternetWriteFile));
  aHooks[ID_InternetSetOptionA] =
    FUN_HOOK(new InternetSetOptionAFB("wininet.dll", "InternetSetOptionA",
                                      &InternetSetOptionA));
  aHooks[ID_HttpAddRequestHeadersA] =
    FUN_HOOK(new HttpAddRequestHeadersAFB("wininet.dll",
                                          "HttpAddRequestHeadersA",
                                          &HttpAddRequestHeadersA));
  aHooks[ID_HttpOpenRequestA] =
    FUN_HOOK(new HttpOpenRequestAFB("wininet.dll", "HttpOpenRequestA",
                                    &HttpOpenRequestA));
  aHooks[ID_HttpQueryInfoA] =
    FUN_HOOK(new HttpQueryInfoAFB("wininet.dll", "HttpQueryInfoA",
                                  &HttpQueryInfoA));
  aHooks[ID_HttpSendRequestA] =
    FUN_HOOK(new HttpSendRequestAFB("wininet.dll", "HttpSendRequestA",
                                    &HttpSendRequestA));
  aHooks[ID_HttpSendRequestExA] =
    FUN_HOOK(new HttpSendRequestExAFB("wininet.dll", "HttpSendRequestExA",
                                      &HttpSendRequestExA));
  aHooks[ID_InternetQueryOptionA] =
    FUN_HOOK(new InternetQueryOptionAFB("wininet.dll", "InternetQueryOptionA",
                                        &InternetQueryOptionA));
  aHooks[ID_InternetErrorDlg] =
    FUN_HOOK(new InternetErrorDlgFB("wininet.dll", "InternetErrorDlg",
                                    InternetErrorDlg));
  aHooks[ID_AcquireCredentialsHandleA] =
    FUN_HOOK(new AcquireCredentialsHandleAFB("sspicli.dll",
                                             "AcquireCredentialsHandleA",
                                             &AcquireCredentialsHandleA));
  aHooks[ID_QueryCredentialsAttributesA] =
    FUN_HOOK(new QueryCredentialsAttributesAFB("sspicli.dll",
                                               "QueryCredentialsAttributesA",
                                               &QueryCredentialsAttributesA));
  aHooks[ID_FreeCredentialsHandle] =
    FUN_HOOK(new FreeCredentialsHandleFB("sspicli.dll",
                                         "FreeCredentialsHandle",
                                         &FreeCredentialsHandle));
#endif // defined(XP_WIN)
}

#undef FUN_HOOK

} // namespace plugins
} // namespace mozilla
