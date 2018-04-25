/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_ipc_PluginHooksWin_h
#define dom_plugins_ipc_PluginHooksWin_h 1

#include <map>
#include <algorithm>
#include <utility>
#include "base/task.h"
#include "mozilla/ipc/ProcessChild.h"
#include "FunctionBrokerChild.h"
#include "mtransport/runnable_utils.h"
#include "PluginMessageUtils.h"
#include "mozilla/Logging.h"
#include "FunctionHook.h"
#include "FunctionBrokerIPCUtils.h"

#if defined(XP_WIN)
#define SECURITY_WIN32
#include <security.h>
#include <wininet.h>
#include <schnlsp.h>
#if defined(MOZ_SANDBOX)
#include "sandboxPermissions.h"
#endif
#endif // defined(XP_WIN)

/**
 * This functionality supports automatic method hooking (FunctionHook) and
 * brokering (FunctionBroker), which are used to intercept system calls
 * (using the nsDllInterceptor) and replace them with new functionality (hook)
 * or proxy them on another process (broker).
 * There isn't much of a public interface to this (see FunctionHook
 * for initialization functionality) since the majority of the behavior
 * comes from intercepting calls to DLL methods (making those DLL methods the
 * public interface).  Generic RPC can be achieved without DLLs or function
 * interception by directly calling the FunctionBroker::InterceptorStub.
 *
 * The system supports the most common logic surrounding brokering by allowing
 * the client to supply strategies for them.  Some examples of common tasks that
 * are supported by automatic brokering:
 *
 * * Intercepting a new Win32 method:
 *
 * Step 1: Add a typedef or subclass of either FunctionHook (non-brokering) or
 * FunctionBroker (automatic brokering) to FunctionBroker.cpp, using a new
 * FunctionHookID (added to that enum).
 * For example:
 *   typedef FunctionBroker<ID_GetKeyState, decltype(GetKeyState)> GetKeyStateFB
 * Use a subclass instead of a typedef if you need to maintain data or state.
 *
 * Step 2: Add an instance of that object to the FunctionHookList in
 * AddFunctionHook(FunctionHookList&) or AddBrokeredFunctionHook(FunctionHookList&).
 * This typically just means calling the constructor with the correct info.
 * At a minimum, this means supplying the names of the DLL and method to
 * broker, and a pointer to the original version of the method.
 * For example:
 *   aHooks[ID_GetKeyState] =
 *     new GetKeyStateFB("user32.dll", "GetKeyState", &GetKeyState);
 *
 * Step 3: If brokering, make sure the system can (un)marshal the parameters,
 * either by the means below or by adding the type to IpdlTuple, which we use
 * for type-safely (un)marshaling the parameter list.
 *
 * * Only brokering _some_ calls to the method:
 *
 * FunctionBroker's constructor allows the user to supply a ShouldBroker
 * function, which takes the parameters of the method call and returns false
 * if we should use the original method instead of brokering.
 *
 * * Only passing _some_ parameters to the brokering process / returning
 *   parameters to client:
 *
 * If a system call changes a parameter call-by-reference style then the
 * parameter's value needs to be returned to the client.  The FunctionBroker
 * has "phase" (request/response) objects that it uses to determine which
 * parameters are sent/returned.  This example tells InternetWriteFileFB to
 * return its third parameter:
 *   template<> template<>
 *   struct InternetWriteFileFB::Response::Info::ShouldMarshal<3> { static const bool value = true; };
 * By default, all parameters have ShouldMarshal set in the request phase
 * and only the return value (parameter -1) has it set in the response phase.
 *
 * * Marshalling special parameter/return types:
 *
 * The IPCTypeMap in FunctionBroker maps a parameter or return type
 * to a type that IpdlTuple knows how to marshal.  By default, the map is
 * the identity but some types need special handling.
 * The map is endpoint-specific (it is a member of the EndpointHandler),
 * so a different type can be used
 * for client -> server and for server -> client.  Note that the
 * types must be able to Copy() from one another -- the default Copy()
 * implementation uses the type's assignment operator.
 * See e.g. EndpointHandler<CLIENT>::IPCTypeMap<LPOPENFILENAMEW>.
 *
 * * Anything more complex involving parameter transmission:
 *
 * Sometimes marshaling parameters can require something more complex.  In
 * those cases, you will need to specialize the Marshal and Unmarshal
 * methods of the request or response handler and perform your complex logic
 * there.  A wise approach is to map your complex parameters into a simpler
 * parameter list and delegate the Marshal/Unmarshal calls to them.  For
 * example, an API might take a void* and an int as a buffer and length.
 * Obviously a void* cannot generally be marshaled.  However,  we can delegate
 * this call to a parameter list that takes a string in place of the buffer and
 * length.  Something like:
 *
 * typedef RequestHandler<ID_HookedFunc,
 *                        int HOOK_CALL (nsDependentCSubstring)> HookedFuncDelegateReq;
 *
 * template<>
 * void HookedFuncFB::Request::Marshal(IpdlTuple& aTuple, const void*& aBuf, const int& aBufLen)
 * {
 *   MOZ_ASSERT(nWritten);
 *   HookedFuncDelegateReq::Marshal(aTuple, nsDependentCSubstring(aBuf, aBufLen));
 * }
 * 
 * template<>
 * bool HookedFuncFB::Request::Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple,
 *                                       void*& aBuf, int& aBufLen)
 * {
 *   nsDependentCSubstring str;
 *   if (!HookedFuncDelegateReq::Unmarshal(aScd, aTuple, str)) {
 *     return false;
 *   }
 *
 *   // Request phase unmarshal uses ServerCallData for dynamically-allocating
 *   // memory.
 *   aScd.AllocateString(str, aBuf, false);
 *   aBufLen = str.Length();
 *   return true;
 * }
 *
 * See e.g. InternetWriteFileFB for a complete example of delegation.
 *
 * * Brokering but need the server to do more than just run the function:
 *
 * Specialize the FunctionBroker's RunFunction.  By default, it just runs
 * the function.  See GetSaveFileNameWFB for an example that does more.
 *
 */

namespace mozilla {
namespace plugins {

#if defined(XP_WIN)

// Currently, all methods we hook use the WINAPI calling convention.
#define HOOK_CALL WINAPI

typedef std::pair<ULONG_PTR, ULONG_PTR> UlongPair;
typedef std::map<UlongPair, uint64_t> UlongPairToIdMap;
extern UlongPairToIdMap sPairToIdMap;
typedef std::map<uint64_t, UlongPair> IdToUlongPairMap;
extern IdToUlongPairMap sIdToPairMap;
typedef std::map<void*, uint64_t> PtrToIdMap;
extern PtrToIdMap sPtrToIdMap;
typedef std::map<uint64_t, void*> IdToPtrMap;
extern IdToPtrMap sIdToPtrMap;

#else // defined(XP_WIN)

// Any methods we hook use the default calling convention.
#define HOOK_CALL 

#endif // defined(XP_WIN)

inline bool IsOdd(uint64_t aVal) { return aVal & 1; }

// This enum is used to track if this process is currently running the client
// or server side of brokering.
enum Endpoint { SERVER, CLIENT };
inline const char *EndpointMsg(Endpoint aVal) { return aVal == SERVER ? "SERVER" : "CLIENT";  }

template <typename ParamType>
inline void LogParameterValue(int aIndex, const ParamType& aParam)
{
  // To avoid overhead, don't do this in release.
#ifdef DEBUG
  if (!MOZ_LOG_TEST(sPluginHooksLog, LogLevel::Verbose)) {
    return;
  }
  std::wstring paramString;
  IPC::LogParam(aParam, &paramString);
  HOOK_LOG(LogLevel::Verbose,
           ("Parameter %d: %S", aIndex, paramString.c_str()));
#endif
}

// This specialization is needed to log the common pattern where null is used
// as a fixed value for a pointer-type that is unknown to IPC.
template <typename ParamType>
inline void LogParameterValue(int aIndex, ParamType* const& aParam)
{
#ifdef DEBUG
  HOOK_LOG(LogLevel::Verbose,
           ("Parameter %d: pointer value - %p", aIndex, aParam));
#endif
}

template <>
inline void LogParameterValue(int aIndex, const nsDependentCSubstring& aParam)
{
#ifdef DEBUG
  HOOK_LOG(LogLevel::Verbose,
           ("Parameter %d : %s", aIndex, FormatBlob(aParam).Data()));
#endif
}

template <>
inline void LogParameterValue(int aIndex, char* const& aParam)
{
#ifdef DEBUG
  // A char* can be a block of raw memory.
  nsDependentCSubstring str;
  if (aParam) {
    str.Rebind(const_cast<char*>(aParam), strnlen(aParam, MAX_BLOB_CHARS_TO_LOG));
  } else {
    str.SetIsVoid(true);
  }
  LogParameterValue(aIndex, str);
#endif
}

template <>
inline void LogParameterValue(int aIndex, const char* const& aParam)
{
#ifdef DEBUG
  LogParameterValue(aIndex, const_cast<char* const&>(aParam));
#endif
}

#if defined(XP_WIN)
template <>
inline void LogParameterValue(int aIndex, const SEC_GET_KEY_FN& aParam)
{
#ifdef DEBUG
  MOZ_ASSERT(aParam == nullptr);
  HOOK_LOG(LogLevel::Verbose, ("Parameter %d: null function.", aIndex));
#endif
}

template <>
inline void LogParameterValue(int aIndex, LPVOID* const & aParam)
{
#ifdef DEBUG
  MOZ_ASSERT(aParam == nullptr);
  HOOK_LOG(LogLevel::Verbose, ("Parameter %d: null void pointer.", aIndex));
#endif
}
#endif // defined(XP_WIN)

// Used to check if a fixed parameter value is equal to the parameter given
// in the original function call.
template <typename ParamType>
inline bool ParameterEquality(const ParamType& aParam1, const ParamType& aParam2)
{
  return aParam1 == aParam2;
}

// Specialization: char* equality is string equality
template <>
inline bool ParameterEquality(char* const& aParam1, char* const& aParam2)
{
  return ((!aParam1 && !aParam2) ||
          (aParam1 && aParam2 && !strcmp(aParam1, aParam2)));
}

// Specialization: const char* const equality is string equality
template <>
inline bool ParameterEquality(const char* const& aParam1, const char* const& aParam2)
{
  return ParameterEquality(const_cast<char* const&>(aParam1), const_cast<char* const&>(aParam2));
}

/**
  * A type map _from_ the type of a parameter in the original function
  * we are brokering _to_ a type that we can marshal.  We must be able
  * to Copy() the marshaled type using the parameter type.
  * The default maps from type T back to type T.
  */
template<typename OrigType> struct IPCTypeMap     { typedef OrigType ipc_type; };
template<> struct IPCTypeMap<char*>               { typedef nsDependentCSubstring ipc_type; };
template<> struct IPCTypeMap<const char*>         { typedef nsDependentCSubstring ipc_type; };
template<> struct IPCTypeMap<long>                { typedef int32_t ipc_type; };
template<> struct IPCTypeMap<unsigned long>       { typedef uint32_t ipc_type; };

#if defined(XP_WIN)
template<> struct IPCTypeMap<PSecHandle>          { typedef uint64_t ipc_type; };
template<> struct IPCTypeMap<PTimeStamp>          { typedef uint64_t ipc_type; };
template<> struct IPCTypeMap<void*>               { typedef uint64_t ipc_type; }; // HANDLEs
template<> struct IPCTypeMap<HWND>                { typedef NativeWindowHandle ipc_type; };
template<> struct IPCTypeMap<PSCHANNEL_CRED>      { typedef IPCSchannelCred ipc_type; };
template<> struct IPCTypeMap<LPINTERNET_BUFFERSA> { typedef IPCInternetBuffers ipc_type; };
template<> struct IPCTypeMap<LPDWORD>             { typedef uint32_t ipc_type; };
#endif

template <typename AllocType>
static void DeleteDestructor(void* aObj) { delete static_cast<AllocType*>(aObj); }

extern void FreeDestructor(void* aObj);

// The ServerCallData is a list of ServerCallItems that should be freed when
// the server has completed a function call and marshaled a response.
class ServerCallData
{
public:
  typedef void (DestructorType)(void*);

  // Allocate a certain type.
  template <typename AllocType>
  AllocType* Allocate(DestructorType* aDestructor = &DeleteDestructor<AllocType>)
  {
    AllocType* ret = new AllocType();
    mList.AppendElement(FreeItem(ret, aDestructor));
    return ret;
  }

  template <typename AllocType>
  AllocType* Allocate(const AllocType& aValueToCopy,
                      DestructorType* aDestructor = &DeleteDestructor<AllocType>)
  {
    AllocType* ret = Allocate<AllocType>(aDestructor);
    *ret = aValueToCopy;
    return ret;
  }

  // Allocate memory, storing the pointer in buf.
  template <typename PtrType>
  void AllocateMemory(unsigned long aBufLen, PtrType& aBuf)
  {
    if (aBufLen) {
      aBuf = static_cast<PtrType>(malloc(aBufLen));
      mList.AppendElement(FreeItem(aBuf, FreeDestructor));
    } else {
      aBuf = nullptr;
    }
  }

  template <typename PtrType>
  void AllocateString(const nsACString& aStr, PtrType& aBuf, bool aCopyNullTerminator=true)
  {
    uint32_t nullByte = aCopyNullTerminator ? 1 : 0;
    char* tempBuf = static_cast<char*>(malloc(aStr.Length() + nullByte));
    memcpy(tempBuf, aStr.Data(), aStr.Length() + nullByte);
    mList.AppendElement(FreeItem(tempBuf, FreeDestructor));
    aBuf = tempBuf;
  }

  // Run the given destructor on the given memory, for special cases where
  // memory is allocated elsewhere but must still be freed.
  void PostDestructor(void* aMem, DestructorType* aDestructor)
  {
    mList.AppendElement(FreeItem(aMem, aDestructor));
  }

#if defined(XP_WIN)
  // Allocate memory and a DWORD block-length, storing them in the
  // corresponding parameters.
  template <typename PtrType>
  void AllocateMemory(DWORD aBufLen, PtrType& aBuf, LPDWORD& aBufLenCopy)
  {
    aBufLenCopy = static_cast<LPDWORD>(malloc(sizeof(DWORD)));
    *aBufLenCopy = aBufLen;
    mList.AppendElement(FreeItem(aBufLenCopy, FreeDestructor));
    AllocateMemory(aBufLen, aBuf);
  }
#endif // defined(XP_WIN)

private:
  // FreeItems are used to free objects that were temporarily needed for
  // dispatch, such as buffers that are given as a parameter.
  class FreeItem
  {
    void* mPtr;
    DestructorType* mDestructor;
    FreeItem(FreeItem& aOther);  // revoked
  public:
    explicit FreeItem(void* aPtr, DestructorType* aDestructor) :
        mPtr(aPtr)
      , mDestructor(aDestructor)
    {
      MOZ_ASSERT(mDestructor || !aPtr);
    }

    FreeItem(FreeItem&& aOther) :
        mPtr(aOther.mPtr)
      , mDestructor(aOther.mDestructor)
    {
      aOther.mPtr = nullptr;
      aOther.mDestructor = nullptr;
    }

    ~FreeItem() { if (mDestructor) { mDestructor(mPtr); } }
  };

  typedef nsTArray<FreeItem> FreeItemList;
  FreeItemList mList;
};

// Holds an IpdlTuple and a ServerCallData.  This is used by the phase handlers
// (RequestHandler and ResponseHandler) in the Unmarshaling phase.
// Server-side unmarshaling (during the request phase) uses a ServerCallData
// to keep track of allocated memory.  In the client, ServerCallDatas are
// not used and that value will always be null.
class IpdlTupleContext
{
public:
  explicit IpdlTupleContext(const IpdlTuple* aTuple, ServerCallData* aScd=nullptr) :
    mTuple(aTuple), mScd(aScd)
  {
    MOZ_ASSERT(aTuple);
  }

  ServerCallData* GetServerCallData() { return mScd; }
  const IpdlTuple* GetIpdlTuple() { return mTuple;  }

private:
  const IpdlTuple* mTuple;
  ServerCallData* mScd;
};

template<typename DestType, typename SrcType>
inline void Copy(DestType& aDest, const SrcType& aSrc)
{
  aDest = (DestType)aSrc;
}

template<>
inline void Copy(nsDependentCSubstring& aDest, const nsDependentCSubstring& aSrc)
{
  if (aSrc.IsVoid()) {
    aDest.SetIsVoid(true);
  } else {
    aDest.Rebind(aSrc.Data(), aSrc.Length());
  }
}

#if defined(XP_WIN)

template<>
inline void Copy(uint64_t& aDest, const PTimeStamp& aSrc)
{
  aDest = static_cast<uint64_t>(aSrc->QuadPart);
}

template<>
inline void Copy(PTimeStamp& aDest, const uint64_t& aSrc)
{
  aDest->QuadPart = static_cast<LONGLONG>(aSrc);
}

#endif // defined(XP_WIN)

template<Endpoint e> struct EndpointHandler;
template<> struct EndpointHandler<CLIENT> {
  static const Endpoint OtherSide = SERVER;

  template<typename DestType, typename SrcType>
  inline static void Copy(ServerCallData* aScd, DestType& aDest, const SrcType& aSrc)
  {
    MOZ_ASSERT(!aScd);    // never used in the CLIENT
    Copy(aDest, aSrc);
  }

  template<typename DestType, typename SrcType>
  inline static void Copy(DestType& aDest, const SrcType& aSrc)
  {
    mozilla::plugins::Copy(aDest, aSrc);
  }
};

#if defined(XP_WIN)

template<>
inline void EndpointHandler<CLIENT>::Copy(uint64_t& aDest, const PSecHandle& aSrc)
{
  MOZ_ASSERT((aSrc->dwLower == aSrc->dwUpper) && IsOdd(aSrc->dwLower));
  aDest = static_cast<uint64_t>(aSrc->dwLower);
}

template<>
inline void EndpointHandler<CLIENT>::Copy(PSecHandle& aDest, const uint64_t& aSrc)
{
  MOZ_ASSERT(IsOdd(aSrc));
  aDest->dwLower = static_cast<ULONG_PTR>(aSrc);
  aDest->dwUpper = static_cast<ULONG_PTR>(aSrc);
}

template<>
inline void EndpointHandler<CLIENT>::Copy(OpenFileNameIPC& aDest, const LPOPENFILENAMEW& aSrc)
{
  aDest.CopyFromOfn(aSrc);
}

template<>
inline void EndpointHandler<CLIENT>::Copy(LPOPENFILENAMEW& aDest, const OpenFileNameRetIPC& aSrc)
{
  aSrc.AddToOfn(aDest);
}

#endif // defined(XP_WIN)

// const char* should be null terminated but this is not always the case.
// In those cases, we must override this default behavior.
template<>
inline void EndpointHandler<CLIENT>::Copy(nsDependentCSubstring& aDest, const char* const& aSrc)
{
  // In the client, we just bind to the caller's string
  if (aSrc) {
    aDest.Rebind(aSrc, strlen(aSrc));
  } else {
    aDest.SetIsVoid(true);
  }
}

template<>
inline void EndpointHandler<CLIENT>::Copy(const char*& aDest, const nsDependentCSubstring& aSrc)
{
  MOZ_ASSERT_UNREACHABLE("Cannot return const parameters.");
}

template<>
inline void EndpointHandler<CLIENT>::Copy(nsDependentCSubstring& aDest, char* const& aSrc)
{
  // In the client, we just bind to the caller's string
  if (aSrc) {
    aDest.Rebind(aSrc, strlen(aSrc));
  } else {
    aDest.SetIsVoid(true);
  }
}

template<>
inline void EndpointHandler<CLIENT>::Copy(char*& aDest,
                                          const nsDependentCSubstring& aSrc)
{
  MOZ_ASSERT_UNREACHABLE("Returning char* parameters is not yet suported.");
}

#if defined(XP_WIN)

template<>
inline void EndpointHandler<CLIENT>::Copy(IPCSchannelCred& aDest,
                                          const PSCHANNEL_CRED& aSrc)
{
  if (aSrc) {
    aDest.CopyFrom(aSrc);
  }
}

template<>
inline void EndpointHandler<CLIENT>::Copy(IPCInternetBuffers& aDest,
                                          const LPINTERNET_BUFFERSA& aSrc)
{
  aDest.CopyFrom(aSrc);
}

template<>
inline void EndpointHandler<CLIENT>::Copy(uint32_t& aDest, const LPDWORD& aSrc)
{
  aDest = *aSrc;
}

template<>
inline void EndpointHandler<CLIENT>::Copy(LPDWORD& aDest, const uint32_t& aSrc)
{
  *aDest = aSrc;
}

#endif // #if defined(XP_WIN)

template<> struct EndpointHandler<SERVER> {
  static const Endpoint OtherSide = CLIENT;

  // Specializations of this method may allocate memory for types that need it
  // during Unmarshaling.  They record the allocation in the ServerCallData.
  // When copying values in the SERVER, we should be sure to carefully validate
  // the information that came from the client as the client may be compromised
  // by malicious code.
  template<typename DestType, typename SrcType>
  inline static void Copy(ServerCallData* aScd, DestType& aDest, const SrcType& aSrc)
  {
    Copy(aDest, aSrc);
  }

  template<typename DestType, typename SrcType>
  inline static void Copy(DestType& aDest, const SrcType& aSrc)
  {
    mozilla::plugins::Copy(aDest, aSrc);
  }
};

template<>
inline void EndpointHandler<SERVER>::Copy(nsDependentCSubstring& aDest, const nsDependentCSubstring& aSrc)
{
  aDest.Rebind(aSrc.Data(), aSrc.Length());
  aDest.SetIsVoid(aSrc.IsVoid());
}

// const char* should be null terminated but this is not always the case.
// In those cases, we override this default behavior.
template<>
inline void EndpointHandler<SERVER>::Copy(nsDependentCSubstring& aDest, const char* const& aSrc)
{
  MOZ_ASSERT_UNREACHABLE("Const parameter cannot be returned by brokering process.");
}

template<>
inline void EndpointHandler<SERVER>::Copy(nsDependentCSubstring& aDest, char* const& aSrc)
{
  MOZ_ASSERT_UNREACHABLE("Returning char* parameters is not yet suported.");
}

#if defined(XP_WIN)

// PSecHandle is the same thing as PCtxtHandle and PCredHandle
template<>
inline void EndpointHandler<SERVER>::Copy(uint64_t& aDest, const PSecHandle& aSrc)
{
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

// HANDLEs and HINTERNETs marshal (for return values)
template<>
inline void EndpointHandler<SERVER>::Copy(uint64_t& aDest, void* const & aSrc)
{
  // If the HANDLE/HINSTANCE was an error then don't store it.
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

// HANDLEs and HINTERNETs unmarshal
template<>
inline void EndpointHandler<SERVER>::Copy(void*& aDest, const uint64_t& aSrc)
{
  aDest = nullptr;
  MOZ_RELEASE_ASSERT(IsOdd(aSrc));

  // If the src is not found in the map then we get aDest == 0
  void* ptr = sIdToPtrMap[aSrc];
  aDest = reinterpret_cast<void*>(ptr);
  MOZ_RELEASE_ASSERT(aDest);
}

template<>
inline void EndpointHandler<SERVER>::Copy(OpenFileNameRetIPC& aDest, const LPOPENFILENAMEW& aSrc)
{
  aDest.CopyFromOfn(aSrc);
}

template<>
inline void EndpointHandler<SERVER>::Copy(PSCHANNEL_CRED& aDest, const IPCSchannelCred& aSrc)
{
  if (aDest) {
    aSrc.CopyTo(aDest);
  }
}

template<>
inline void EndpointHandler<SERVER>::Copy(uint32_t& aDest, const LPDWORD& aSrc)
{
  aDest = *aSrc;
}

template<>
inline void EndpointHandler<SERVER>::Copy(LPDWORD& aDest, const uint32_t& aSrc)
{
  MOZ_RELEASE_ASSERT(aDest);
  *aDest = aSrc;
}

#endif // defined(XP_WIN)

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, char*& aDest, const nsDependentCSubstring& aSrc)
{
  // In the parent, we must allocate the string.
  MOZ_ASSERT(aScd);
  if (aSrc.IsVoid()) {
    aDest = nullptr;
    return;
  }
  aScd->AllocateMemory(aSrc.Length() + 1, aDest);
  memcpy(aDest, aSrc.Data(), aSrc.Length());
  aDest[aSrc.Length()] = '\0';
}

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, const char*& aDest, const nsDependentCSubstring& aSrc)
{
  char* nonConstDest;
  Copy(aScd, nonConstDest, aSrc);
  aDest = nonConstDest;
}

#if defined(XP_WIN)

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, PSecHandle& aDest, const uint64_t& aSrc)
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

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, PTimeStamp& aDest, const uint64_t& aSrc)
{
  MOZ_ASSERT(!aDest);
  aDest = aScd->Allocate<::TimeStamp>();
  Copy(aDest, aSrc);
}

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, LPOPENFILENAMEW& aDest, const OpenFileNameIPC& aSrc)
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

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, PSCHANNEL_CRED& aDest, const IPCSchannelCred& aSrc)
{
  MOZ_ASSERT(!aDest);
  aDest = aScd->Allocate<SCHANNEL_CRED>();
  Copy(aDest, aSrc);
}

template<>
inline void EndpointHandler<SERVER>::Copy(ServerCallData* aScd, LPINTERNET_BUFFERSA& aDest, const IPCInternetBuffers& aSrc)
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

#endif // defined(XP_WIN)

// PhaseHandler is a RequestHandler or a ResponseHandler.
template<Endpoint endpoint, typename PhaseHandler>
struct Marshaler
{
  // Driver
  template<int firstIndex = 0, typename ... VarParams>
  static void Marshal(IpdlTuple& aMarshaledTuple,
                         const VarParams&... aParams)
  {
    MarshalParameters<firstIndex>(aMarshaledTuple, aParams...);
  }

  // Driver
  template<int firstIndex = 0, typename ... VarParams>
  static bool Unmarshal(IpdlTupleContext& aUnmarshaledTuple, VarParams&... aParams)
  {
    return UnmarshalParameters<firstIndex>(aUnmarshaledTuple, 0, aParams...);
  }

  template<int paramIndex, typename OrigType,
           bool shouldMarshal = PhaseHandler::Info::template ShouldMarshal<paramIndex>::value>
  struct MaybeMarshalParameter {};

  /**
   * shouldMarshal = true case
   */
  template<int paramIndex, typename OrigType>
  struct MaybeMarshalParameter<paramIndex, OrigType, true>
  {
    template<typename IPCType = typename PhaseHandler::template IPCTypeMap<OrigType>::ipc_type>
    static void MarshalParameter(IpdlTuple& aMarshaledTuple, const OrigType& aParam)
    {
      HOOK_LOG(LogLevel::Verbose,
               ("%s marshaling parameter %d.", EndpointMsg(endpoint), paramIndex));
      IPCType ipcObject;
      EndpointHandler<endpoint>::Copy(ipcObject, aParam);  // Must be able to Copy() from OrigType to IPCType
      LogParameterValue(paramIndex, ipcObject);
      aMarshaledTuple.AddElement(ipcObject);
    }
  };

  /**
   * shouldMarshal = false case
   */
  template<int paramIndex, typename OrigType>
  struct MaybeMarshalParameter<paramIndex, OrigType, false>
  {
    static void MarshalParameter(IpdlTuple& aMarshaledTuple, const OrigType& aParam)
    {
      HOOK_LOG(LogLevel::Verbose,
               ("%s not marshaling parameter %d.", EndpointMsg(endpoint), paramIndex));
    }
  };

  /**
   * Recursive case: marshals aFirstParam to aMarshaledTuple (if desired),
   * then marshals the aRemainingParams.
   */
  template<int paramIndex,
           typename VarParam,
           typename ... VarParams>
  static void MarshalParameters(IpdlTuple& aMarshaledTuple,
                         const VarParam& aFirstParam,
                         const VarParams&... aRemainingParams)
  {
    MaybeMarshalParameter<paramIndex, VarParam>::MarshalParameter(aMarshaledTuple, aFirstParam);
    MarshalParameters<paramIndex + 1, VarParams...>(aMarshaledTuple,
                                              aRemainingParams...);
  }

  /**
   * Base case: empty parameter list -- nothing to marshal.
   */
  template <int paramIndex> static void MarshalParameters(IpdlTuple& aMarshaledTuple) {}

  template<int tupleIndex, typename OrigType,
           bool shouldMarshal = PhaseHandler::Info::template ShouldMarshal<tupleIndex>::value,
           bool hasFixedValue = PhaseHandler::Info::template HasFixedValue<tupleIndex>::value>
  struct MaybeUnmarshalParameter {};

  /**
   * ShouldMarshal = true case.  HasFixedValue must be false in that case.
   */
  template<int tupleIndex, typename VarParam>
  struct MaybeUnmarshalParameter<tupleIndex, VarParam, true, false>
  {
    template<typename IPCType = typename PhaseHandler::template IPCTypeMap<VarParam>::ipc_type>
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple, int& aNextTupleIdx, VarParam& aParam)
    {
      const IPCType* ipcObject = aUnmarshaledTuple.GetIpdlTuple()->Element<IPCType>(aNextTupleIdx);
      if (!ipcObject) {
        HOOK_LOG(LogLevel::Error,
                 ("%s failed to unmarshal parameter %d.", EndpointMsg(endpoint), tupleIndex));
        return false;
      }
      HOOK_LOG(LogLevel::Verbose,
               ("%s unmarshaled parameter %d.", EndpointMsg(endpoint), tupleIndex));
      LogParameterValue(tupleIndex, *ipcObject);
      EndpointHandler<endpoint>::Copy(aUnmarshaledTuple.GetServerCallData(), aParam, *ipcObject);
      ++aNextTupleIdx;
      return true;
    }
  };

  /**
   * ShouldMarshal = true : nsDependentCSubstring specialization
   */
  template<int tupleIndex>
  struct MaybeUnmarshalParameter<tupleIndex, nsDependentCSubstring, true, false>
  {
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple, int& aNextTupleIdx, nsDependentCSubstring& aParam)
    {
      // Deserialize as an nsCString and then copy the info into the nsDependentCSubstring
      const nsCString* ipcObject = aUnmarshaledTuple.GetIpdlTuple()->Element<nsCString>(aNextTupleIdx);
      if (!ipcObject) {
        HOOK_LOG(LogLevel::Error,
                 ("%s failed to unmarshal parameter %d.", EndpointMsg(endpoint), tupleIndex));
        return false;
      }
      HOOK_LOG(LogLevel::Verbose,
               ("%s unmarshaled parameter %d.", EndpointMsg(endpoint), tupleIndex));

      aParam.Rebind(ipcObject->Data(), ipcObject->Length());
      aParam.SetIsVoid(ipcObject->IsVoid());
      LogParameterValue(tupleIndex, aParam);
      ++aNextTupleIdx;
      return true;
    }
  };

  /**
   * ShouldMarshal = true : char* specialization
   */
  template<int tupleIndex>
  struct MaybeUnmarshalParameter<tupleIndex, char*, true, false>
  {
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple, int& aNextTupleIdx, char*& aParam)
    {
      nsDependentCSubstring tempStr;
      bool ret = MaybeUnmarshalParameter<tupleIndex, nsDependentCSubstring, true, false>::UnmarshalParameter(aUnmarshaledTuple, aNextTupleIdx, tempStr);
      EndpointHandler<endpoint>::Copy(aUnmarshaledTuple.GetServerCallData(), aParam, tempStr);
      return ret;
    }
  };

  /**
   * ShouldMarshal = true : const char* specialization
   */
  template<int tupleIndex>
  struct MaybeUnmarshalParameter<tupleIndex, const char*, true, false>
  {
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple, int& aNextTupleIdx, const char*& aParam)
    {
      char* tempStr;
      bool ret = MaybeUnmarshalParameter<tupleIndex, char*, true, false>::UnmarshalParameter(aUnmarshaledTuple, aNextTupleIdx, tempStr);
      aParam = tempStr;
      return ret;
    }
  };

  /**
   * ShouldMarshal = false, fixed parameter case
   */
  template<int tupleIndex, typename VarParam>
  struct MaybeUnmarshalParameter<tupleIndex, VarParam, false, true>
  {
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple,  int& aNextTupleIdx, VarParam& aParam) {
      // Copy default value if this is client->server communication (and if it exists)
      PhaseHandler::template CopyFixedParam<tupleIndex, VarParam>(aParam);
      HOOK_LOG(LogLevel::Verbose,
               ("%s parameter %d not unmarshaling -- using fixed value.", EndpointMsg(endpoint), tupleIndex));
      LogParameterValue(tupleIndex, aParam);
      return true;
    }
  };

  /**
   * ShouldMarshal = false, unfixed parameter case.  Assume user has done special handling.
   */
  template<int tupleIndex, typename VarParam>
  struct MaybeUnmarshalParameter<tupleIndex, VarParam, false, false>
  {
    static inline bool UnmarshalParameter(IpdlTupleContext& aUnmarshaledTuple,  int& aNextTupleIdx, VarParam& aParam) {
      HOOK_LOG(LogLevel::Verbose,
               ("%s parameter %d not automatically unmarshaling.", EndpointMsg(endpoint), tupleIndex));
      // DLP: TODO: specializations fail LogParameterValue(tupleIndex, aParam);
      return true;
    }
  };

  /**
   * Recursive case: unmarshals aFirstParam to aUnmarshaledTuple (if desired),
   * then unmarshals the aRemainingParams.
   * The endpoint specifies the side this process is on: client or server.
   */
  template <int tupleIndex,
            typename VarParam,
            typename ... VarParams>
  static bool UnmarshalParameters(IpdlTupleContext& aUnmarshaledTuple, int aNextTupleIdx,
                           VarParam& aFirstParam, VarParams&... aRemainingParams)
  {
    // TODO: DLP: I currently increment aNextTupleIdx in the method (its a reference).  This is awful.
    if (!MaybeUnmarshalParameter<tupleIndex, VarParam>::UnmarshalParameter(aUnmarshaledTuple, aNextTupleIdx, aFirstParam)) {
      return false;
    }
    return UnmarshalParameters<tupleIndex + 1, VarParams...>(aUnmarshaledTuple, aNextTupleIdx, aRemainingParams...);
  }

  /**
   * Base case: empty parameter list -- nothing to unmarshal.
   */
  template <int>
  static bool UnmarshalParameters(IpdlTupleContext& aUnmarshaledTuple, int aNextTupleIdx)
  {
    return true;
  }
};


// The default marshals all parameters.
template<FunctionHookId functionId> struct RequestInfo
{
  template<int paramIndex> struct FixedValue;

  template<int paramIndex, typename = int> struct HasFixedValue { static const bool value = false; };
  template<int paramIndex> struct HasFixedValue<paramIndex, decltype(FixedValue<paramIndex>::value,0)>
  {
    static const bool value = true;
  };

  // By default we the request should marshal any non-fixed parameters.
  template<int paramIndex>
  struct ShouldMarshal { static const bool value = !HasFixedValue<paramIndex>::value; };
};

/**
 * This base stores the RequestHandler's IPCTypeMap.  It really only
 * exists to circumvent the arbitrary C++ rule (enforced by mingw) forbidding
 * full class specialization of a class (IPCTypeMap<T>) inside of an
 * unspecialized template class (RequestHandler<T>).p
 */
struct RequestHandlerBase
{
  // Default to the namespace-level IPCTypeMap
  template<typename OrigType>
  struct IPCTypeMap
  {
    typedef typename mozilla::plugins::IPCTypeMap<OrigType>::ipc_type ipc_type;
  };
};

#if defined(XP_WIN)

// Request phase uses OpenFileNameIPC for an LPOPENFILENAMEW parameter.
template<>
struct RequestHandlerBase::IPCTypeMap<LPOPENFILENAMEW> { typedef OpenFileNameIPC ipc_type; };

#endif // defined(XP_WIN)

template<FunctionHookId functionId, typename FunctionType> struct RequestHandler;

template<FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
struct RequestHandler<functionId, ResultType HOOK_CALL (ParamTypes...)> :
  public RequestHandlerBase
{
  typedef ResultType(HOOK_CALL FunctionType)(ParamTypes...);
  typedef RequestHandler<functionId, FunctionType> SelfType;
  typedef RequestInfo<functionId> Info;

  static void Marshal(IpdlTuple& aTuple, const ParamTypes&... aParams)
  {
    ReqMarshaler::Marshal(aTuple, aParams...);
  }

  static bool Unmarshal(ServerCallData& aScd, const IpdlTuple& aTuple, ParamTypes&... aParams)
  {
    IpdlTupleContext cxt(&aTuple, &aScd);
    return ReqUnmarshaler::Unmarshal(cxt, aParams...);
  }

  typedef Marshaler<CLIENT, SelfType> ReqMarshaler;
  typedef Marshaler<SERVER, SelfType> ReqUnmarshaler;

  /**
   * Returns true if a call made with the given parameters should be
   * brokered (vs. passed-through to the original function).
   */
  static bool ShouldBroker(Endpoint aEndpoint, const ParamTypes&... aParams)
  {
    // True if all filtered parameters match their filter value.
    return CheckFixedParams(aParams...);
  }

  template <int paramIndex, typename VarParam>
  static void CopyFixedParam(VarParam& aParam)
  {
    aParam = Info::template FixedValue<paramIndex>::value;
  }

protected:
  // Returns true if filtered parameters match their filter value.
  static bool CheckFixedParams(const ParamTypes&... aParams)
  {
    return CheckFixedParamsHelper<0>(aParams...);
  }

  // If no FixedValue<paramIndex> is defined and equal to FixedType then always pass.
  template<int paramIndex, typename = int>
  struct CheckFixedParam
  {
    template<typename ParamType>
    static inline bool Check(const ParamType& aParam) { return true; }
  };

  // If FixedValue<paramIndex> is defined then check equality.
  template<int paramIndex>
  struct CheckFixedParam<paramIndex, decltype(Info::template FixedValue<paramIndex>::value,0)>
  {
    template<typename ParamType>
    static inline bool Check(ParamType& aParam)
    {
      return ParameterEquality(aParam, Info::template FixedValue<paramIndex>::value);
    }
  };

  // Recursive case: Chcek head parameter, then tail parameters.
  template<int index, typename VarParam, typename ... VarParams>
  static bool CheckFixedParamsHelper(const VarParam& aParam, const VarParams&... aParams)
  {
    if (!CheckFixedParam<index>::Check(aParam)) {
      return false;     // didn't match a fixed parameter
    }
    return CheckFixedParamsHelper<index + 1>(aParams...);
  }

  // Base case: All fixed parameters matched.
  template<int> static bool CheckFixedParamsHelper() { return true; }
};

// The default returns no parameters -- only the return value.
template<FunctionHookId functionId>
struct ResponseInfo
{
  template<int paramIndex> struct HasFixedValue
  {
    static const bool value = RequestInfo<functionId>::template HasFixedValue<paramIndex>::value;
  };

  // Only the return value (index -1) is sent by default.
  template<int paramIndex> struct ShouldMarshal { static const bool value = (paramIndex == -1); };

  // This is the condition on the function result that we use to determine if
  // the windows thread-local error state should be sent to the client.  The
  // error is typically only relevant if the function did not succeed.
  template<typename ResultType>
  static bool ShouldTransmitError(const ResultType& aResult) {
    return !static_cast<bool>(aResult);
  }
};

/**
 * Same rationale as for RequestHandlerBase.
 */
struct ResponseHandlerBase
{
  // Default to the namespace-level IPCTypeMap
  template<typename OrigType>
  struct IPCTypeMap
  {
    typedef typename mozilla::plugins::IPCTypeMap<OrigType>::ipc_type ipc_type;
  };
};

#if defined(XP_WIN)

// Response phase uses OpenFileNameRetIPC for an LPOPENFILENAMEW parameter.
template<>
struct ResponseHandlerBase::IPCTypeMap<LPOPENFILENAMEW> { typedef OpenFileNameRetIPC ipc_type; };

#endif

template<FunctionHookId functionId, typename FunctionType> struct ResponseHandler;

template<FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
struct ResponseHandler<functionId, ResultType HOOK_CALL (ParamTypes...)> :
  public ResponseHandlerBase
{
  typedef ResultType(HOOK_CALL FunctionType)(ParamTypes...);
  typedef ResponseHandler<functionId, FunctionType> SelfType;
  typedef ResponseInfo<functionId> Info;

  static void Marshal(IpdlTuple& aTuple, const ResultType& aResult, const ParamTypes&... aParams)
  {
    // Note that this "trick" means that the first parameter we marshal is
    // considered to be parameter #-1 when checking the ResponseInfo.
    // The parameters in the list therefore start at index 0.
    RspMarshaler::template Marshal<-1>(aTuple, aResult, aParams...);
  }
  static bool Unmarshal(const IpdlTuple& aTuple, ResultType& aResult, ParamTypes&... aParams)
  {
    IpdlTupleContext cxt(&aTuple);
    return RspUnmarshaler::template Unmarshal<-1>(cxt, aResult, aParams...);
  }

  typedef Marshaler<SERVER, SelfType> RspMarshaler;
  typedef Marshaler<CLIENT, SelfType> RspUnmarshaler;

  // Fixed parameters are not used in the response phase.
  template <int tupleIndex, typename VarParam>
  static void CopyFixedParam(VarParam& aParam) {}
};

/**
 * Reference-counted monitor, used to synchronize communication between a
 * thread using a brokered API and the FunctionDispatch thread.
 */
class FDMonitor : public Monitor
{
public:
  FDMonitor() : Monitor("FunctionDispatchThread lock")
  {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FDMonitor)

private:
  ~FDMonitor() {}
};

/**
 * Data for hooking a function that we automatically broker in a remote
 * process.
 */
template <FunctionHookId functionId, typename FunctionType>
class FunctionBroker;

template <FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
class FunctionBroker<functionId, ResultType HOOK_CALL (ParamTypes...)> :
  public BasicFunctionHook<functionId, ResultType HOOK_CALL (ParamTypes...)>
{
public:
  typedef Tuple<ParamTypes...> TupleParamTypes;
  typedef Tuple<mozilla::Maybe<ParamTypes>...> TupleMaybeParamTypes;
  typedef Tuple<ParamTypes*...> TupleParamPtrTypes;
  typedef Tuple<ParamTypes&...> TupleParamRefTypes;
  static const size_t numParams = sizeof...(ParamTypes);

  typedef ResultType (HOOK_CALL FunctionType)(ParamTypes...);
  typedef FunctionBroker<functionId, FunctionType> SelfType;
  typedef BasicFunctionHook<functionId, FunctionType> FunctionHookInfoType;
  typedef FunctionHookInfoType BaseType;

  typedef RequestHandler<functionId, FunctionType> Request;
  typedef ResponseHandler<functionId, FunctionType> Response;

  FunctionBroker(const char* aModuleName, const char* aMethodName,
                       FunctionType* aOriginalFunction) :
    BasicFunctionHook<functionId, FunctionType>(aModuleName, aMethodName,
                                                aOriginalFunction, InterceptorStub)
  {
  }

  // This is the function used to replace the original DLL-intercepted function.
  static ResultType HOOK_CALL InterceptorStub(ParamTypes... aParams)
  {
    MOZ_ASSERT(functionId < FunctionHook::GetHooks()->Length());
    FunctionHook* self = FunctionHook::GetHooks()->ElementAt(functionId);
    MOZ_ASSERT(self && self->FunctionId() == functionId);
    const SelfType* broker = static_cast<const SelfType*>(self);
    return broker->MaybeBrokerCallClient(aParams...);
  }

  /**
   * Handle a call by running the original version or brokering, depending on
   * ShouldBroker.  All parameter types (including the result type)
   * must have IPDL ParamTraits specializations or appear in this object's
   * IPCTypeMap.  If brokering fails for any reason then this falls back to
   * calling the original version of the function.
   */
  ResultType MaybeBrokerCallClient(ParamTypes&... aParameters) const;

  /**
   * Called server-side to run the original function using aInTuple
   * as parameter values.  The return value and returned parameters
   * (in that order) are added to aOutTuple.
   */
  bool
  RunOriginalFunction(base::ProcessId aClientId, const IPC::IpdlTuple &aInTuple,
                      IPC::IpdlTuple *aOutTuple) const override
  {
    return BrokerCallServer(aClientId, aInTuple, aOutTuple);
  }

protected:
  bool BrokerCallServer(base::ProcessId aClientId, const IpdlTuple &aInTuple,
                        IpdlTuple *aOutTuple) const
  {
    return BrokerCallServer(aClientId, aInTuple, aOutTuple,
                             std::index_sequence_for<ParamTypes...>{});
  }

  bool BrokerCallClient(uint32_t& aWinError, ResultType& aResult, ParamTypes&... aParameters) const;
  bool PostToDispatchThread(uint32_t& aWinError, ResultType& aRet, ParamTypes&... aParameters) const;

  static void
  PostToDispatchHelper(const SelfType* bmhi, RefPtr<FDMonitor> monitor, bool* notified,
                       bool* ok, uint32_t* winErr, ResultType* r, ParamTypes*... p)
  {
    // Note: p is also non-null... its just hard to assert that.
    MOZ_ASSERT(bmhi && monitor && notified && ok && winErr && r);
    MOZ_ASSERT(*notified == false);
    *ok = bmhi->BrokerCallClient(*winErr, *r, *p...);

    {
      // We need to grab the lock to make sure that Wait() has been
      // called in PostToDispatchThread.  We need that since we wake it with
      // Notify().
      MonitorAutoLock lock(*monitor);
      *notified = true;
    }

    monitor->Notify();
  };

  template<typename ... VarParams>
  ResultType
  RunFunction(FunctionType* aFunction, base::ProcessId aClientId,
                VarParams&... aParams) const
  {
    return aFunction(aParams...);
  };

  bool BrokerCallServer(base::ProcessId aClientId, const IpdlTuple &aInTuple,
                        IpdlTuple *aOutTuple, ParamTypes&... aParams) const;

  template<size_t... Indices>
  bool BrokerCallServer(base::ProcessId aClientId, const IpdlTuple &aInTuple,
                         IpdlTuple *aOutTuple, std::index_sequence<Indices...>) const
  {
    TupleParamTypes paramTuple;
    return BrokerCallServer(aClientId, aInTuple, aOutTuple,
                             Get<Indices>(paramTuple)...);
  }
};

template <FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
ResultType
FunctionBroker<functionId, ResultType HOOK_CALL (ParamTypes...)>::MaybeBrokerCallClient(ParamTypes&... aParameters) const
{
  MOZ_ASSERT(FunctionBrokerChild::GetInstance());

  // Broker the call if ShouldBroker says to.  Otherwise, or if brokering
  // fails, then call the original implementation.
  if (!FunctionBrokerChild::GetInstance()) {
    HOOK_LOG(LogLevel::Error,
             ("[%s] Client attempted to broker call without actor.", FunctionHookInfoType::mFunctionName.Data()));
  }
  else if (Request::ShouldBroker(CLIENT, aParameters...)) {
    HOOK_LOG(LogLevel::Debug,
             ("[%s] Client attempting to broker call.", FunctionHookInfoType::mFunctionName.Data()));
    uint32_t winError;
    ResultType ret;
    bool success = BrokerCallClient(winError, ret, aParameters...);
    HOOK_LOG(LogLevel::Info,
             ("[%s] Client brokering %s.", FunctionHookInfoType::mFunctionName.Data(), SuccessMsg(success)));
    if (success) {
#if defined(XP_WIN)
      if (Response::Info::ShouldTransmitError(ret)) {
        HOOK_LOG(LogLevel::Debug,
                 ("[%s] Client setting thread error code: %08x.", FunctionHookInfoType::mFunctionName.Data(), winError));
        ::SetLastError(winError);
      }
#endif
      return ret;
    }
  }

  HOOK_LOG(LogLevel::Info,
            ("[%s] Client could not broker.  Running original version.", FunctionHookInfoType::mFunctionName.Data()));
  return FunctionHookInfoType::mOldFunction(aParameters...);
}

template <FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
bool
FunctionBroker<functionId, ResultType HOOK_CALL (ParamTypes...)>::BrokerCallClient(uint32_t& aWinError,
                                                                  ResultType& aResult,
                                                                  ParamTypes&... aParameters) const
{
  if (!FunctionBrokerChild::GetInstance()->IsDispatchThread()) {
    return PostToDispatchThread(aWinError, aResult, aParameters...);
  }

  if (FunctionBrokerChild::GetInstance()) {
    IpdlTuple sending, returned;
    HOOK_LOG(LogLevel::Debug,
             ("[%s] Client marshaling parameters.", FunctionHookInfoType::mFunctionName.Data()));
    Request::Marshal(sending, aParameters...);
    HOOK_LOG(LogLevel::Info,
             ("[%s] Client sending broker message.", FunctionHookInfoType::mFunctionName.Data()));
    if (FunctionBrokerChild::GetInstance()->SendBrokerFunction(FunctionHookInfoType::FunctionId(), sending,
                                                           &returned)) {
      HOOK_LOG(LogLevel::Debug,
               ("[%s] Client received broker message response.", FunctionHookInfoType::mFunctionName.Data()));
      bool success = Response::Unmarshal(returned, aResult, aParameters...);
      HOOK_LOG(LogLevel::Info,
               ("[%s] Client response unmarshaling: %s.", FunctionHookInfoType::mFunctionName.Data(), SuccessMsg(success)));
#if defined(XP_WIN)
      if (success && Response::Info::ShouldTransmitError(aResult)) {
        uint32_t* winError = returned.Element<UINT32>(returned.NumElements()-1);
        if (!winError) {
          HOOK_LOG(LogLevel::Error,
                    ("[%s] Client failed to unmarshal error code.", FunctionHookInfoType::mFunctionName.Data()));
          return false;
        }
        HOOK_LOG(LogLevel::Debug,
                 ("[%s] Client response unmarshaled error code: %08x.",
                 FunctionHookInfoType::mFunctionName.Data(), *winError));
        aWinError = *winError;
      }
#endif
      return success;
    }
  }

  HOOK_LOG(LogLevel::Error,
            ("[%s] Client failed to broker call.", FunctionHookInfoType::mFunctionName.Data()));
  return false;
}

template <FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
bool
FunctionBroker<functionId, ResultType HOOK_CALL (ParamTypes...)>::BrokerCallServer(base::ProcessId aClientId, const IpdlTuple &aInTuple,
                  IpdlTuple *aOutTuple, ParamTypes&... aParams) const
{
  HOOK_LOG(LogLevel::Info, ("[%s] Server brokering function.", FunctionHookInfoType::mFunctionName.Data()));

  ServerCallData scd;
  if (!Request::Unmarshal(scd, aInTuple, aParams...)) {
    HOOK_LOG(LogLevel::Info,
             ("[%s] Server failed to unmarshal.", FunctionHookInfoType::mFunctionName.Data()));
    return false;
  }

  // Make sure that this call was legal -- do not execute a call that
  // shouldn't have been brokered in the first place.
  if (!Request::ShouldBroker(SERVER, aParams...)) {
    HOOK_LOG(LogLevel::Error,
             ("[%s] Server rejected brokering request.", FunctionHookInfoType::mFunctionName.Data()));
    return false;
  }

  // Run the function we are brokering.
  HOOK_LOG(LogLevel::Info, ("[%s] Server broker running function.", FunctionHookInfoType::mFunctionName.Data()));
  ResultType ret = RunFunction(FunctionHookInfoType::mOldFunction, aClientId, aParams...);

#if defined(XP_WIN)
  // Record the thread-local error state (before it is changed) if needed.
  uint32_t err = UINT_MAX;
  bool transmitError = Response::Info::ShouldTransmitError(ret);
  if (transmitError) {
    err = ::GetLastError();
    HOOK_LOG(LogLevel::Info,
             ("[%s] Server returning thread error code: %08x.", FunctionHookInfoType::mFunctionName.Data(), err));
  }
#endif

  // Add the result, win thread error and any returned parameters to the returned tuple.
  Response::Marshal(*aOutTuple, ret, aParams...);
#if defined(XP_WIN)
  if (transmitError) {
    aOutTuple->AddElement(err);
  }
#endif

  return true;
}

template <FunctionHookId functionId, typename ResultType, typename ... ParamTypes>
bool
FunctionBroker<functionId,ResultType HOOK_CALL (ParamTypes...)>::
PostToDispatchThread(uint32_t& aWinError, ResultType& aRet,
                     ParamTypes&... aParameters) const
{
  MOZ_ASSERT(!FunctionBrokerChild::GetInstance()->IsDispatchThread());
  HOOK_LOG(LogLevel::Debug,
           ("Posting broker task '%s' to dispatch thread", FunctionHookInfoType::mFunctionName.Data()));

  // Run PostToDispatchHelper on the dispatch thread.  It will notify our
  // waiting monitor when it is done.
  RefPtr<FDMonitor> monitor(new FDMonitor());
  MonitorAutoLock lock(*monitor);
  bool success = false;
  bool notified = false;
  FunctionBrokerChild::GetInstance()->PostToDispatchThread(
    NewRunnableFunction("FunctionDispatchThreadRunnable", &PostToDispatchHelper,
                        this, monitor, &notified, &success, &aWinError, &aRet,
                        &aParameters...));

  // We wait to be notified, testing that notified was actually set to make
  // sure this isn't a spurious wakeup.
  while (!notified) {
    monitor->Wait();
  }
  return success;
}

void AddBrokeredFunctionHooks(FunctionHookArray& aHooks);

} // namespace plugins
} // namespace mozilla

#endif // dom_plugins_ipc_PluginHooksWin_h
