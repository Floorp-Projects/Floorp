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
#include <sddl.h>
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

// Specialization of EndpointHandlers for Flash file dialog brokering.
struct FileDlgEHContainer
{
  template<Endpoint e> struct EndpointHandler;
};

template<>
struct FileDlgEHContainer::EndpointHandler<CLIENT> :
  public BaseEndpointHandler<CLIENT, FileDlgEHContainer::EndpointHandler<CLIENT>>
{
  using BaseEndpointHandler<CLIENT, EndpointHandler<CLIENT>>::Copy;

  inline static void Copy(OpenFileNameIPC& aDest, const LPOPENFILENAMEW& aSrc)
  {
    aDest.CopyFromOfn(aSrc);
  }
  inline static void Copy(LPOPENFILENAMEW& aDest, const OpenFileNameRetIPC& aSrc)
  {
    aSrc.AddToOfn(aDest);
  }
};

template<>
struct FileDlgEHContainer::EndpointHandler<SERVER> :
  public BaseEndpointHandler<SERVER, FileDlgEHContainer::EndpointHandler<SERVER>>
{
  using BaseEndpointHandler<SERVER, EndpointHandler<SERVER>>::Copy;

  inline static void Copy(OpenFileNameRetIPC& aDest, const LPOPENFILENAMEW& aSrc)
  {
    aDest.CopyFromOfn(aSrc);
  }
  inline static void Copy(ServerCallData* aScd, LPOPENFILENAMEW& aDest, const OpenFileNameIPC& aSrc)
  {
    MOZ_ASSERT(!aDest);
    ServerCallData::DestructorType* destructor =
      [](void* aObj) {
      OpenFileNameIPC::FreeOfnStrings(static_cast<LPOPENFILENAMEW>(aObj));
      DeleteDestructor<OPENFILENAMEW>(aObj);
    };
    aDest = aScd->Allocate<OPENFILENAMEW>(destructor);
    aSrc.AllocateOfnStrings(aDest);
    aSrc.AddToOfn(aDest);
  }
};

// FunctionBroker type that uses FileDlgEHContainer
template <FunctionHookId functionId, typename FunctionType>
using FileDlgFunctionBroker = FunctionBroker<functionId, FunctionType, FileDlgEHContainer>;

// Specialization of EndpointHandlers for Flash SSL brokering.
struct SslEHContainer {
  template<Endpoint e> struct EndpointHandler;
};

template<>
struct SslEHContainer::EndpointHandler<CLIENT> :
  public BaseEndpointHandler<CLIENT, SslEHContainer::EndpointHandler<CLIENT>>
{
  using BaseEndpointHandler<CLIENT, EndpointHandler<CLIENT>>::Copy;

  inline static void Copy(uint64_t& aDest, const PSecHandle& aSrc)
  {
    MOZ_ASSERT((aSrc->dwLower == aSrc->dwUpper) && IsOdd(aSrc->dwLower));
    aDest = static_cast<uint64_t>(aSrc->dwLower);
  }
  inline static void Copy(PSecHandle& aDest, const uint64_t& aSrc)
  {
    MOZ_ASSERT(IsOdd(aSrc));
    aDest->dwLower = static_cast<ULONG_PTR>(aSrc);
    aDest->dwUpper = static_cast<ULONG_PTR>(aSrc);
  }
  inline static void Copy(IPCSchannelCred& aDest, const PSCHANNEL_CRED& aSrc)
  {
    if (aSrc) {
      aDest.CopyFrom(aSrc);
    }
  }
  inline static void Copy(IPCInternetBuffers& aDest, const LPINTERNET_BUFFERSA& aSrc)
  {
    aDest.CopyFrom(aSrc);
  }
};

template<>
struct SslEHContainer::EndpointHandler<SERVER> :
  public BaseEndpointHandler<SERVER, SslEHContainer::EndpointHandler<SERVER>>
{
  using BaseEndpointHandler<SERVER, EndpointHandler<SERVER>>::Copy;

  // PSecHandle is the same thing as PCtxtHandle and PCredHandle.
  inline static void Copy(uint64_t& aDest, const PSecHandle& aSrc)
  {
    // If the SecHandle was an error then don't store it.
    if (!aSrc) {
      aDest = 0;
      return;
    }

    static uint64_t sNextVal = 1;
    UlongPair key(aSrc->dwLower, aSrc->dwUpper);
    // Fetch val by reference to update the value in the map
    uint64_t& val = sPairToIdMap[key];
    if (val == 0) {
      MOZ_ASSERT(IsOdd(sNextVal));
      val = sNextVal;
      sIdToPairMap[val] = key;
      sNextVal += 2;
    }
    aDest = val;
  }

  // HANDLEs and HINTERNETs marshal with obfuscation (for return values)
  inline static void Copy(uint64_t& aDest, void* const & aSrc)
  {
    // If the HANDLE/HINTERNET was an error then don't store it.
    if (!aSrc) {
      aDest = 0;
      return;
    }

    static uint64_t sNextVal = 1;
    // Fetch val by reference to update the value in the map
    uint64_t& val = sPtrToIdMap[aSrc];
    if (val == 0) {
      MOZ_ASSERT(IsOdd(sNextVal));
      val = sNextVal;
      sIdToPtrMap[val] = aSrc;
      sNextVal += 2;
    }
    aDest = val;
  }

  // HANDLEs and HINTERNETs unmarshal with obfuscation
  inline static void Copy(void*& aDest, const uint64_t& aSrc)
  {
    aDest = nullptr;
    MOZ_RELEASE_ASSERT(IsOdd(aSrc));

    // If the src is not found in the map then we get aDest == 0
    void* ptr = sIdToPtrMap[aSrc];
    aDest = reinterpret_cast<void*>(ptr);
    MOZ_RELEASE_ASSERT(aDest);
  }

  inline static void Copy(PSCHANNEL_CRED& aDest, const IPCSchannelCred& aSrc)
  {
    if (aDest) {
      aSrc.CopyTo(aDest);
    }
  }

  inline static void Copy(ServerCallData* aScd, PSecHandle& aDest, const uint64_t& aSrc)
  {
    MOZ_ASSERT(!aDest);
    MOZ_RELEASE_ASSERT(IsOdd(aSrc));

    // If the src is not found in the map then we get the pair { 0, 0 }
    aDest = aScd->Allocate<SecHandle>();
    const UlongPair& pair = sIdToPairMap[aSrc];
    MOZ_RELEASE_ASSERT(pair.first || pair.second);
    aDest->dwLower = pair.first;
    aDest->dwUpper = pair.second;
  }

  inline static void Copy(ServerCallData* aScd, PSCHANNEL_CRED& aDest, const IPCSchannelCred& aSrc)
  {
    MOZ_ASSERT(!aDest);
    aDest = aScd->Allocate<SCHANNEL_CRED>();
    Copy(aDest, aSrc);
  }

  inline static void Copy(ServerCallData* aScd, LPINTERNET_BUFFERSA& aDest, const IPCInternetBuffers& aSrc)
  {
    MOZ_ASSERT(!aDest);
    aSrc.CopyTo(aDest);
    ServerCallData::DestructorType* destructor =
      [](void* aObj) {
      LPINTERNET_BUFFERSA inetBuf = static_cast<LPINTERNET_BUFFERSA>(aObj);
      IPCInternetBuffers::FreeBuffers(inetBuf);
      FreeDestructor(inetBuf);
    };
    aScd->PostDestructor(aDest, destructor);
  }
};

// FunctionBroker type that uses SslEHContainer
template <FunctionHookId functionId, typename FunctionType>
using SslFunctionBroker = FunctionBroker<functionId, FunctionType, SslEHContainer>;

/* GetKeyState */

typedef FunctionBroker<ID_GetKeyState, decltype(GetKeyState)> GetKeyStateFB;

template<>
ShouldHookFunc* const
GetKeyStateFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_GETKEYSTATE>;

/* SetCursorPos */

typedef FunctionBroker<ID_SetCursorPos,
                       decltype(SetCursorPos)> SetCursorPosFB;

/* GetSaveFileNameW */

typedef FileDlgFunctionBroker<ID_GetSaveFileNameW,
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

typedef FileDlgFunctionBroker<ID_GetOpenFileNameW,
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

typedef SslFunctionBroker<ID_InternetOpenA,
                          decltype(InternetOpenA)> InternetOpenAFB;

template<>
ShouldHookFunc* const
InternetOpenAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

/* InternetConnectA */

typedef SslFunctionBroker<ID_InternetConnectA,
                          decltype(InternetConnectA)> InternetConnectAFB;

template<>
ShouldHookFunc* const
InternetConnectAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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

typedef SslFunctionBroker<ID_InternetCloseHandle,
                          decltype(InternetCloseHandle)> InternetCloseHandleFB;

template<>
ShouldHookFunc* const
InternetCloseHandleFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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

typedef SslFunctionBroker<ID_InternetQueryDataAvailable,
                          decltype(InternetQueryDataAvailable)> InternetQueryDataAvailableFB;

template<>
ShouldHookFunc* const
InternetQueryDataAvailableFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef InternetQueryDataAvailableFB::Request IQDAReq;
typedef InternetQueryDataAvailableFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET)> IQDADelegateReq;

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

typedef SslFunctionBroker<ID_InternetReadFile,
                          decltype(InternetReadFile)> InternetReadFileFB;

template<>
ShouldHookFunc* const
InternetReadFileFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef InternetReadFileFB::Request IRFRequestHandler;
typedef InternetReadFileFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET, DWORD)> IRFDelegateReq;

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

typedef InternetReadFileFB::Response IRFResponseHandler;
typedef InternetReadFileFB::ResponseDelegate<BOOL HOOK_CALL (nsDependentCSubstring)> IRFDelegateResponseHandler;

// Marshal the output parameter that we sent to the response delegate.
template<> template<>
struct IRFResponseHandler::Info::ShouldMarshal<0> { static const bool value = true; };

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

typedef SslFunctionBroker<ID_InternetWriteFile,
                          decltype(InternetWriteFile)> InternetWriteFileFB;

template<>
ShouldHookFunc* const
InternetWriteFileFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef InternetWriteFileFB::Request IWFReqHandler;
typedef InternetWriteFileFB::RequestDelegate<int HOOK_CALL (HINTERNET, nsDependentCSubstring)> IWFDelegateReqHandler;

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

typedef SslFunctionBroker<ID_InternetSetOptionA,
                          decltype(InternetSetOptionA)> InternetSetOptionAFB;

template<>
ShouldHookFunc* const
InternetSetOptionAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef InternetSetOptionAFB::Request ISOAReqHandler;
typedef InternetSetOptionAFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET, DWORD, nsDependentCSubstring)> ISOADelegateReqHandler;

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

typedef SslFunctionBroker<ID_HttpAddRequestHeadersA,
                          decltype(HttpAddRequestHeadersA)> HttpAddRequestHeadersAFB;

template<>
ShouldHookFunc* const
HttpAddRequestHeadersAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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

typedef SslFunctionBroker<ID_HttpOpenRequestA,
                          decltype(HttpOpenRequestA)> HttpOpenRequestAFB;

template<>
ShouldHookFunc* const
HttpOpenRequestAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef HttpOpenRequestAFB::Request HORAReqHandler;
typedef HttpOpenRequestAFB::RequestDelegate<HINTERNET HOOK_CALL (HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, nsTArray<nsCString>, DWORD, DWORD_PTR)> HORADelegateReqHandler;

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

typedef SslFunctionBroker<ID_HttpQueryInfoA,
                          decltype(HttpQueryInfoA)> HttpQueryInfoAFB;

template<>
ShouldHookFunc* const
HttpQueryInfoAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef HttpQueryInfoAFB::Request HQIARequestHandler;
typedef HttpQueryInfoAFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET, DWORD, BOOL, DWORD, BOOL, DWORD)> HQIADelegateRequestHandler;

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
typedef HttpQueryInfoAFB::ResponseDelegate<BOOL HOOK_CALL (nsDependentCSubstring, DWORD, DWORD)> HQIADelegateResponseHandler;

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

typedef SslFunctionBroker<ID_HttpSendRequestA,
                          decltype(HttpSendRequestA)> HttpSendRequestAFB;

template<>
ShouldHookFunc* const
HttpSendRequestAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef HttpSendRequestAFB::Request HSRARequestHandler;
typedef HttpSendRequestAFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET, nsDependentCSubstring, nsDependentCSubstring)> HSRADelegateRequestHandler;

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

typedef SslFunctionBroker<ID_HttpSendRequestExA,
                          decltype(HttpSendRequestExA)> HttpSendRequestExAFB;

template<>
ShouldHookFunc* const
HttpSendRequestExAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef RequestInfo<ID_HttpSendRequestExA> HSRExAReqInfo;

template<> template<>
struct HSRExAReqInfo::FixedValue<2> { static const LPINTERNET_BUFFERSA value; };
const LPINTERNET_BUFFERSA HSRExAReqInfo::FixedValue<2>::value = nullptr;

// Docs for HttpSendRequestExA say this parameter 'must' be zero but Flash
// passes other values.
// template<> template<>
// struct HSRExAReqInfo::FixedValue<3> { static const DWORD value = 0; };

template<> template<>
struct HSRExAReqInfo::FixedValue<4> { static const DWORD_PTR value; };
const DWORD_PTR HSRExAReqInfo::FixedValue<4>::value = 0;

/* HttpEndRequestA */

typedef SslFunctionBroker<ID_HttpEndRequestA,
  decltype(HttpEndRequestA)> HttpEndRequestAFB;

template<>
ShouldHookFunc* const
HttpEndRequestAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef RequestInfo<ID_HttpEndRequestA> HERAReqInfo;

template<> template<>
struct HERAReqInfo::FixedValue<1> { static const LPINTERNET_BUFFERSA value; };
const LPINTERNET_BUFFERSA HERAReqInfo::FixedValue<1>::value = nullptr;

template<> template<>
struct HERAReqInfo::FixedValue<2> { static const DWORD value; };
const DWORD HERAReqInfo::FixedValue<2>::value = 0;

template<> template<>
struct HERAReqInfo::FixedValue<3> { static const DWORD_PTR value; };
const DWORD_PTR HERAReqInfo::FixedValue<3>::value = 0;

/* InternetQueryOptionA */

typedef SslFunctionBroker<ID_InternetQueryOptionA,
                          decltype(InternetQueryOptionA)> InternetQueryOptionAFB;

template<>
ShouldHookFunc* const
InternetQueryOptionAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

typedef InternetQueryOptionAFB::Request IQOARequestHandler;
typedef InternetQueryOptionAFB::RequestDelegate<BOOL HOOK_CALL (HINTERNET, DWORD, DWORD)> IQOADelegateRequestHandler;

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
typedef InternetQueryOptionAFB::ResponseDelegate<BOOL HOOK_CALL (nsDependentCSubstring, DWORD)> IQOADelegateResponseHandler;

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

typedef SslFunctionBroker<ID_InternetErrorDlg,
                          decltype(InternetErrorDlg)> InternetErrorDlgFB;

template<>
ShouldHookFunc* const
InternetErrorDlgFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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

typedef SslFunctionBroker<ID_AcquireCredentialsHandleA,
                          decltype(AcquireCredentialsHandleA)> AcquireCredentialsHandleAFB;

template<>
ShouldHookFunc* const
AcquireCredentialsHandleAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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
typedef AcquireCredentialsHandleAFB::RequestDelegate<SECURITY_STATUS HOOK_CALL (LPSTR, LPSTR, unsigned long, void*, PSCHANNEL_CRED, SEC_GET_KEY_FN, void*)> ACHADelegateRequestHandler;

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

typedef SslFunctionBroker<ID_QueryCredentialsAttributesA,
                          decltype(QueryCredentialsAttributesA)> QueryCredentialsAttributesAFB;

template<>
ShouldHookFunc* const
QueryCredentialsAttributesAFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

/* FreeCredentialsHandle */

typedef SslFunctionBroker<ID_FreeCredentialsHandle,
                          decltype(FreeCredentialsHandle)> FreeCredentialsHandleFB;

template<>
ShouldHookFunc* const
FreeCredentialsHandleFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_SSL>;

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

/* CreateMutexW */

// Get the user's SID as a string.  Returns an empty string on failure.
static std::wstring GetUserSid()
{
  std::wstring ret;
  // Get user SID from process token information
  HANDLE token;
  BOOL success = ::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token);
  if (!success) {
    return ret;
  }
  DWORD bufLen;
  success = ::GetTokenInformation(token, TokenUser, nullptr, 0, &bufLen);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return ret;
  }
  void* buf = malloc(bufLen);
  success = ::GetTokenInformation(token, TokenUser, buf, bufLen, &bufLen);
  MOZ_ASSERT(success);
  if (success) {
    TOKEN_USER* tokenUser = static_cast<TOKEN_USER*>(buf);
    PSID sid = tokenUser->User.Sid;
    LPWSTR sidStr;
    success = ::ConvertSidToStringSid(sid, &sidStr);
    if (success) {
      ret = sidStr;
      ::LocalFree(sidStr);
    }
  }
  free(buf);
  ::CloseHandle(token);
  return ret;
}

// Get the name Windows uses for the camera mutex.  Returns an empty string
// on failure.
// The camera mutex is identified in Windows code using a hard-coded GUID string,
// "eed3bd3a-a1ad-4e99-987b-d7cb3fcfa7f0", and the user's SID.  The GUID
// value was determined by investigating Windows code.  It is referenced in
// CCreateSwEnum::CCreateSwEnum(void) in devenum.dll.
static std::wstring GetCameraMutexName()
{
  std::wstring userSid = GetUserSid();
  if (userSid.empty()) {
    return userSid;
  }
  return std::wstring(L"eed3bd3a-a1ad-4e99-987b-d7cb3fcfa7f0 - ") + userSid;
}

typedef FunctionBroker<ID_CreateMutexW, decltype(CreateMutexW)> CreateMutexWFB;

template<>
ShouldHookFunc* const
CreateMutexWFB::BaseType::mShouldHook = &CheckQuirks<QUIRK_FLASH_HOOK_CREATEMUTEXW>;

typedef CreateMutexWFB::Request CMWReqHandler;
typedef CMWReqHandler::Info CMWReqInfo;
typedef CreateMutexWFB::Response CMWRspHandler;

template<>
bool CMWReqHandler::ShouldBroker(Endpoint endpoint,
                                 const LPSECURITY_ATTRIBUTES& aAttribs,
                                 const BOOL& aOwner,
                                 const LPCWSTR& aName)
{
  // Statically hold the camera mutex name so that we dont recompute it for
  // every CreateMutexW call in the client process.
  static std::wstring camMutexName = GetCameraMutexName();

  // Only broker if we are requesting the camera mutex.  Note that we only
  // need to check that the client is actually requesting the camera.  The
  // command is always valid on the server as long as we can construct the
  // mutex name.
  if (endpoint == SERVER) {
    return !camMutexName.empty();
  }

  return (!aOwner) && aName && (!camMutexName.empty()) && (camMutexName == aName);
}

// We dont need to marshal any parameters.  We construct all of them server-side.
template<> template<>
struct CMWReqInfo::ShouldMarshal<0> { static const bool value = false; };
template<> template<>
struct CMWReqInfo::ShouldMarshal<1> { static const bool value = false; };
template<> template<>
struct CMWReqInfo::ShouldMarshal<2> { static const bool value = false; };

template<> template<>
HANDLE CreateMutexWFB::RunFunction(CreateMutexWFB::FunctionType* aOrigFunction,
                                   base::ProcessId aClientId,
                                   LPSECURITY_ATTRIBUTES& aAttribs,
                                   BOOL& aOwner,
                                   LPCWSTR& aName) const
{
  // Use CreateMutexW to get the camera mutex and DuplicateHandle to open it
  // for use in the child process.
  // Recall that aAttribs, aOwner and aName are all unmarshaled so they are
  // unassigned garbage.
  SECURITY_ATTRIBUTES mutexAttrib =
    { sizeof(SECURITY_ATTRIBUTES), nullptr /* ignored */, TRUE };
  std::wstring camMutexName = GetCameraMutexName();
  if (camMutexName.empty()) {
    return 0;
  }
  HANDLE serverMutex = ::CreateMutexW(&mutexAttrib, FALSE, camMutexName.c_str());
  if (serverMutex == 0) {
    return 0;
  }
  ScopedProcessHandle clientProcHandle;
  if (!base::OpenProcessHandle(aClientId, &clientProcHandle.rwget())) {
    return 0;
  }
  HANDLE ret;
  if (!::DuplicateHandle(::GetCurrentProcess(), serverMutex, clientProcHandle,
                         &ret, SYNCHRONIZE, FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return 0;
  }
  return ret;
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
  aHooks[ID_HttpEndRequestA] =
    FUN_HOOK(new HttpEndRequestAFB("wininet.dll", "HttpEndRequestA",
      &HttpEndRequestA));
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
  aHooks[ID_CreateMutexW] =
    FUN_HOOK(new CreateMutexWFB("kernel32.dll", "CreateMutexW", &CreateMutexW));
#endif // defined(XP_WIN)
}

#undef FUN_HOOK

} // namespace plugins
} // namespace mozilla
