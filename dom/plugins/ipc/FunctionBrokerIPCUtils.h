#ifndef dom_plugins_ipc_functionbrokeripcutils_h
#define dom_plugins_ipc_functionbrokeripcutils_h 1

#include "PluginMessageUtils.h"

#if defined(XP_WIN)

#define SECURITY_WIN32
#include <security.h>
#include <wininet.h>
#include <schannel.h>
#include <commdlg.h>

#endif // defined(XP_WIN)

namespace mozilla {
namespace plugins {

/**
 * This enum represents all of the methods hooked by the main facility in BrokerClient.
 * It is used to allow quick lookup in the sFunctionsToHook structure.
 */
enum FunctionHookId
{
#if defined(XP_WIN)
    ID_GetWindowInfo = 0
  , ID_GetKeyState
  , ID_SetCursorPos
  , ID_GetSaveFileNameW
  , ID_GetOpenFileNameW
  , ID_InternetOpenA
  , ID_InternetConnectA
  , ID_InternetCloseHandle
  , ID_InternetQueryDataAvailable
  , ID_InternetReadFile
  , ID_InternetWriteFile
  , ID_InternetSetOptionA
  , ID_HttpAddRequestHeadersA
  , ID_HttpOpenRequestA
  , ID_HttpQueryInfoA
  , ID_HttpSendRequestA
  , ID_HttpSendRequestExA
  , ID_InternetQueryOptionA
  , ID_InternetErrorDlg
  , ID_AcquireCredentialsHandleA
  , ID_QueryCredentialsAttributesA
  , ID_FreeCredentialsHandle
  , ID_PrintDlgW
  , ID_FunctionHookCount
#else // defined(XP_WIN)
    ID_FunctionHookCount
#endif // defined(XP_WIN)
};

// Max number of bytes to show when logging a blob of raw memory
static const uint32_t MAX_BLOB_CHARS_TO_LOG = 12;

// Format strings for safe logging despite the fact that they are sometimes
// used as raw binary blobs.
inline nsCString FormatBlob(const nsACString& aParam)
{
  if (aParam.IsVoid() || aParam.IsEmpty()) {
    return nsCString(aParam.IsVoid() ? "<void>" : "<empty>");
  }

  nsCString str;
  uint32_t totalLen = std::min(MAX_BLOB_CHARS_TO_LOG, aParam.Length());
  // If we are printing only a portion of the string then follow it with ellipsis
  const char* maybeEllipsis = (MAX_BLOB_CHARS_TO_LOG < aParam.Length()) ? "..." : "";
  for (uint32_t idx = 0; idx < totalLen; ++idx) {
    // Should be %02x but I've run into a AppendPrintf bug...
    str.AppendPrintf("0x%2x ", aParam.Data()[idx] & 0xff);
  }
  str.AppendPrintf("%s   | '", maybeEllipsis);
  for (uint32_t idx = 0; idx < totalLen; ++idx) {
    str.AppendPrintf("%c", (aParam.Data()[idx] > 0) ? aParam.Data()[idx] : '.');
  }
  str.AppendPrintf("'%s", maybeEllipsis);
  return str;
}

#if defined(XP_WIN)

// Values indicate GetOpenFileNameW and GetSaveFileNameW.
enum GetFileNameFunc { OPEN_FUNC, SAVE_FUNC };

typedef nsTArray<nsCString> StringArray;

// IPC-capable version of the Windows OPENFILENAMEW struct.
typedef struct _OpenFileNameIPC
{
  // Allocates memory for the strings in this object.  This should usually
  // be used with a zeroed out OPENFILENAMEW structure.
  void AllocateOfnStrings(LPOPENFILENAMEW aLpofn) const;
  static void FreeOfnStrings(LPOPENFILENAMEW aLpofn);
  void AddToOfn(LPOPENFILENAMEW aLpofn) const;
  void CopyFromOfn(LPOPENFILENAMEW aLpofn);
  bool operator==(const _OpenFileNameIPC& o) const
  {
    return (o.mHwndOwner == mHwndOwner) &&
           (o.mFilter == mFilter) &&
           (o.mHasFilter == mHasFilter) &&
           (o.mCustomFilterIn == mCustomFilterIn) &&
           (o.mHasCustomFilter == mHasCustomFilter) &&
           (o.mNMaxCustFilterOut == mNMaxCustFilterOut) &&
           (o.mFilterIndex == mFilterIndex) &&
           (o.mFile == mFile) &&
           (o.mNMaxFile == mNMaxFile) &&
           (o.mNMaxFileTitle == mNMaxFileTitle) &&
           (o.mInitialDir == mInitialDir) &&
           (o.mHasInitialDir == mHasInitialDir) &&
           (o.mTitle == mTitle) &&
           (o.mHasTitle == mHasTitle) &&
           (o.mFlags == mFlags) &&
           (o.mDefExt == mDefExt) &&
           (o.mHasDefExt == mHasDefExt) &&
           (o.mFlagsEx == mFlagsEx);
  }

  NativeWindowHandle mHwndOwner;
  std::wstring mFilter;    // Double-NULL terminated (i.e. L"\0\0") if mHasFilter is true
  bool mHasFilter;
  std::wstring mCustomFilterIn;
  bool mHasCustomFilter;
  uint32_t mNMaxCustFilterOut;
  uint32_t mFilterIndex;
  std::wstring mFile;
  uint32_t mNMaxFile;
  uint32_t mNMaxFileTitle;
  std::wstring mInitialDir;
  bool mHasInitialDir;
  std::wstring mTitle;
  bool mHasTitle;
  uint32_t mFlags;
  std::wstring mDefExt;
  bool mHasDefExt;
  uint32_t mFlagsEx;
} OpenFileNameIPC;

// GetOpenFileNameW and GetSaveFileNameW overwrite fields of their OPENFILENAMEW
// parameter.  This represents those values so that they can be returned via IPC.
typedef struct _OpenFileNameRetIPC
{
  void CopyFromOfn(LPOPENFILENAMEW aLpofn);
  void AddToOfn(LPOPENFILENAMEW aLpofn) const;
  bool operator==(const _OpenFileNameRetIPC& o) const
  {
    return (o.mCustomFilterOut == mCustomFilterOut) &&
           (o.mFile == mFile) &&
           (o.mFileTitle == mFileTitle) &&
           (o.mFileOffset == mFileOffset) &&
           (o.mFileExtension == mFileExtension);
  }

  std::wstring mCustomFilterOut;
  std::wstring mFile;    // Double-NULL terminated (i.e. L"\0\0")
  std::wstring mFileTitle;
  uint16_t mFileOffset;
  uint16_t mFileExtension;
} OpenFileNameRetIPC;

typedef struct _IPCSchannelCred
{
  void CopyFrom(const PSCHANNEL_CRED& aSCred);
  void CopyTo(PSCHANNEL_CRED& aSCred) const;
  bool operator==(const _IPCSchannelCred& o) const
  {
    return (o.mEnabledProtocols == mEnabledProtocols) &&
           (o.mMinStrength == mMinStrength) &&
           (o.mMaxStrength == mMaxStrength) &&
           (o.mFlags == mFlags);
  }


  DWORD mEnabledProtocols;
  DWORD mMinStrength;
  DWORD mMaxStrength;
  DWORD mFlags;
} IPCSchannelCred;

typedef struct _IPCInternetBuffers
{
  void CopyFrom(const LPINTERNET_BUFFERSA& aBufs);
  void CopyTo(LPINTERNET_BUFFERSA& aBufs) const;
  bool operator==(const _IPCInternetBuffers& o) const
  {
    return o.mBuffers == mBuffers;
  }
  static void FreeBuffers(LPINTERNET_BUFFERSA& aBufs);

  struct Buffer
  {
    nsCString mHeader;
    uint32_t mHeaderTotal;
    nsCString mBuffer;
    uint32_t mBufferTotal;
    bool operator==(const Buffer& o) const
    {
      return (o.mHeader == mHeader) && (o.mHeaderTotal == mHeaderTotal) &&
             (o.mBuffer == mBuffer) && (o.mBufferTotal == mBufferTotal);
    }
  };
  nsTArray<Buffer> mBuffers;
} IPCInternetBuffers;

typedef struct _IPCPrintDlg
{
  void CopyFrom(const LPPRINTDLGW& aDlg);
  void CopyTo(LPPRINTDLGW& aDlg) const;
  bool operator==(const _IPCPrintDlg& o) const
  {
    MOZ_ASSERT_UNREACHABLE("DLP: TODO:"); return false;
  }
} IPCPrintDlg;

#endif // defined(XP_WIN)

} // namespace plugins
} // namespace mozilla

namespace IPC {

using mozilla::plugins::FunctionHookId;

#if defined(XP_WIN)

using mozilla::plugins::OpenFileNameIPC;
using mozilla::plugins::OpenFileNameRetIPC;
using mozilla::plugins::IPCSchannelCred;
using mozilla::plugins::IPCInternetBuffers;
using mozilla::plugins::IPCPrintDlg;
using mozilla::plugins::NativeWindowHandle;
using mozilla::plugins::StringArray;

template <>
struct ParamTraits<OpenFileNameIPC>
{
  typedef OpenFileNameIPC paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHwndOwner);
    WriteParam(aMsg, aParam.mFilter);
    WriteParam(aMsg, aParam.mHasFilter);
    WriteParam(aMsg, aParam.mCustomFilterIn);
    WriteParam(aMsg, aParam.mHasCustomFilter);
    WriteParam(aMsg, aParam.mNMaxCustFilterOut);
    WriteParam(aMsg, aParam.mFilterIndex);
    WriteParam(aMsg, aParam.mFile);
    WriteParam(aMsg, aParam.mNMaxFile);
    WriteParam(aMsg, aParam.mNMaxFileTitle);
    WriteParam(aMsg, aParam.mInitialDir);
    WriteParam(aMsg, aParam.mHasInitialDir);
    WriteParam(aMsg, aParam.mTitle);
    WriteParam(aMsg, aParam.mHasTitle);
    WriteParam(aMsg, aParam.mFlags);
    WriteParam(aMsg, aParam.mDefExt);
    WriteParam(aMsg, aParam.mHasDefExt);
    WriteParam(aMsg, aParam.mFlagsEx);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->mHwndOwner) &&
        ReadParam(aMsg, aIter, &aResult->mFilter) &&
        ReadParam(aMsg, aIter, &aResult->mHasFilter) &&
        ReadParam(aMsg, aIter, &aResult->mCustomFilterIn) &&
        ReadParam(aMsg, aIter, &aResult->mHasCustomFilter) &&
        ReadParam(aMsg, aIter, &aResult->mNMaxCustFilterOut) &&
        ReadParam(aMsg, aIter, &aResult->mFilterIndex) &&
        ReadParam(aMsg, aIter, &aResult->mFile) &&
        ReadParam(aMsg, aIter, &aResult->mNMaxFile) &&
        ReadParam(aMsg, aIter, &aResult->mNMaxFileTitle) &&
        ReadParam(aMsg, aIter, &aResult->mInitialDir) &&
        ReadParam(aMsg, aIter, &aResult->mHasInitialDir) &&
        ReadParam(aMsg, aIter, &aResult->mTitle) &&
        ReadParam(aMsg, aIter, &aResult->mHasTitle) &&
        ReadParam(aMsg, aIter, &aResult->mFlags) &&
        ReadParam(aMsg, aIter, &aResult->mDefExt) &&
        ReadParam(aMsg, aIter, &aResult->mHasDefExt) &&
        ReadParam(aMsg, aIter, &aResult->mFlagsEx)) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%S, %S, %S, %S]", aParam.mFilter.c_str(),
                              aParam.mCustomFilterIn.c_str(), aParam.mFile.c_str(),
                              aParam.mTitle.c_str()));
  }
};

template <>
struct ParamTraits<OpenFileNameRetIPC>
{
  typedef OpenFileNameRetIPC paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCustomFilterOut);
    WriteParam(aMsg, aParam.mFile);
    WriteParam(aMsg, aParam.mFileTitle);
    WriteParam(aMsg, aParam.mFileOffset);
    WriteParam(aMsg, aParam.mFileExtension);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->mCustomFilterOut) &&
        ReadParam(aMsg, aIter, &aResult->mFile) &&
        ReadParam(aMsg, aIter, &aResult->mFileTitle) &&
        ReadParam(aMsg, aIter, &aResult->mFileOffset) &&
        ReadParam(aMsg, aIter, &aResult->mFileExtension)) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%S, %S, %S, %d, %d]", aParam.mCustomFilterOut.c_str(),
                              aParam.mFile.c_str(), aParam.mFileTitle.c_str(),
                              aParam.mFileOffset, aParam.mFileExtension));
  }
};

template <>
struct ParamTraits<mozilla::plugins::GetFileNameFunc> :
  public ContiguousEnumSerializerInclusive<mozilla::plugins::GetFileNameFunc,
                                           mozilla::plugins::OPEN_FUNC,
                                           mozilla::plugins::SAVE_FUNC>
{};

template <>
struct ParamTraits<IPCSchannelCred>
{
  typedef mozilla::plugins::IPCSchannelCred paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mEnabledProtocols));
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mMinStrength));
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mMaxStrength));
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mFlags));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint32_t proto, minStr, maxStr, flags;
    if (!ReadParam(aMsg, aIter, &proto) ||
        !ReadParam(aMsg, aIter, &minStr) ||
        !ReadParam(aMsg, aIter, &maxStr) ||
        !ReadParam(aMsg, aIter, &flags)) {
      return false;
    }
    aResult->mEnabledProtocols = proto;
    aResult->mMinStrength = minStr;
    aResult->mMaxStrength = maxStr;
    aResult->mFlags = flags;
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d,%d,%d,%d]",
                              aParam.mEnabledProtocols, aParam.mMinStrength,
                              aParam.mMaxStrength, aParam.mFlags));
  }
};

template <>
struct ParamTraits<IPCInternetBuffers::Buffer>
{
  typedef mozilla::plugins::IPCInternetBuffers::Buffer paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHeader);
    WriteParam(aMsg, aParam.mHeaderTotal);
    WriteParam(aMsg, aParam.mBuffer);
    WriteParam(aMsg, aParam.mBufferTotal);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mHeader) &&
           ReadParam(aMsg, aIter, &aResult->mHeaderTotal) &&
           ReadParam(aMsg, aIter, &aResult->mBuffer) &&
           ReadParam(aMsg, aIter, &aResult->mBufferTotal);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    nsCString head = mozilla::plugins::FormatBlob(aParam.mHeader);
    nsCString buffer = mozilla::plugins::FormatBlob(aParam.mBuffer);
    std::string msg = StringPrintf("[%s, %d, %s, %d]",
                                   head.Data(), aParam.mHeaderTotal,
                                   buffer.Data(), aParam.mBufferTotal);
    aLog->append(msg.begin(), msg.end());
  }
};

template <>
struct ParamTraits<IPCInternetBuffers>
{
  typedef mozilla::plugins::IPCInternetBuffers paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBuffers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mBuffers);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    ParamTraits<nsTArray<IPCInternetBuffers::Buffer>>::Log(aParam.mBuffers, aLog);
  }
};

template <>
struct ParamTraits<IPCPrintDlg>
{
  typedef mozilla::plugins::IPCPrintDlg paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    MOZ_ASSERT_UNREACHABLE("TODO: DLP:");
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    MOZ_ASSERT_UNREACHABLE("TODO: DLP:");
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    MOZ_ASSERT_UNREACHABLE("TODO: DLP:");
  }
};

#endif // defined(XP_WIN)

template <>
struct ParamTraits<FunctionHookId> :
  public ContiguousEnumSerializer<FunctionHookId,
                                  static_cast<FunctionHookId>(0),
                                  FunctionHookId::ID_FunctionHookCount>
{};

} // namespace IPC

#endif /* dom_plugins_ipc_functionbrokeripcutils_h */
