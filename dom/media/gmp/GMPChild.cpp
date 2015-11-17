/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPChild.h"
#include "GMPContentChild.h"
#include "GMPProcessChild.h"
#include "GMPLoader.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "GMPAudioDecoderChild.h"
#include "GMPDecryptorChild.h"
#include "GMPVideoHost.h"
#include "nsDebugImpl.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"
#include "mozilla/dom/CrashReporterChild.h"
#include "GMPUtils.h"
#include "prio.h"

using mozilla::dom::CrashReporterChild;

static const int MAX_VOUCHER_LENGTH = 500000;

#ifdef XP_WIN
#include <stdlib.h> // for _exit()
#else
#include <unistd.h> // for _exit()
#endif

#if defined(MOZ_GMP_SANDBOX)
#if defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif

namespace mozilla {

#undef LOG
#undef LOGD

extern LogModule* GetGMPLog();
#define LOG(level, x, ...) MOZ_LOG(GetGMPLog(), (level), (x, ##__VA_ARGS__))
#define LOGD(x, ...) LOG(mozilla::LogLevel::Debug, "GMPChild[pid=%d] " x, (int)base::GetCurrentProcId(), ##__VA_ARGS__)

namespace gmp {

GMPChild::GMPChild()
  : mAsyncShutdown(nullptr)
  , mGMPMessageLoop(MessageLoop::current())
  , mGMPLoader(nullptr)
{
  LOGD("GMPChild ctor");
  nsDebugImpl::SetMultiprocessMode("GMP");
}

GMPChild::~GMPChild()
{
  LOGD("GMPChild dtor");
}

static bool
GetFileBase(const nsAString& aPluginPath,
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
            nsCOMPtr<nsIFile>& aLibDirectory,
#endif
            nsCOMPtr<nsIFile>& aFileBase,
            nsAutoString& aBaseName)
{
  nsresult rv = NS_NewLocalFile(aPluginPath,
                                true, getter_AddRefs(aFileBase));
  if (NS_FAILED(rv)) {
    return false;
  }

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  if (NS_FAILED(aFileBase->Clone(getter_AddRefs(aLibDirectory)))) {
    return false;
  }
#endif

  nsCOMPtr<nsIFile> parent;
  rv = aFileBase->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_FAILED(rv)) {
    return false;
  }

  aBaseName = Substring(parentLeafName,
                        4,
                        parentLeafName.Length() - 1);
  return true;
}

static bool
GetPluginFile(const nsAString& aPluginPath,
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
              nsCOMPtr<nsIFile>& aLibDirectory,
#endif
              nsCOMPtr<nsIFile>& aLibFile)
{
  nsAutoString baseName;
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  GetFileBase(aPluginPath, aLibDirectory, aLibFile, baseName);
#else
  GetFileBase(aPluginPath, aLibFile, baseName);
#endif

#if defined(XP_MACOSX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".dylib");
#elif defined(OS_POSIX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".so");
#elif defined(XP_WIN)
  nsAutoString binaryName =                            baseName + NS_LITERAL_STRING(".dll");
#else
#error not defined
#endif
  aLibFile->AppendRelativePath(binaryName);
  return true;
}

#ifdef XP_WIN
static bool
GetInfoFile(const nsAString& aPluginPath,
            nsCOMPtr<nsIFile>& aInfoFile)
{
  nsAutoString baseName;
  GetFileBase(aPluginPath, aInfoFile, baseName);
  nsAutoString infoFileName = baseName + NS_LITERAL_STRING(".info");
  aInfoFile->AppendRelativePath(infoFileName);
  return true;
}
#endif

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
static bool
GetPluginPaths(const nsAString& aPluginPath,
               nsCString &aPluginDirectoryPath,
               nsCString &aPluginFilePath)
{
  nsCOMPtr<nsIFile> libDirectory, libFile;
  if (!GetPluginFile(aPluginPath, libDirectory, libFile)) {
    return false;
  }

  // Mac sandbox rules expect paths to actual files and directories -- not
  // soft links.
  bool isLink;
  libDirectory->IsSymlink(&isLink);
  if (isLink) {
    libDirectory->GetNativeTarget(aPluginDirectoryPath);
  } else {
    libDirectory->GetNativePath(aPluginDirectoryPath);
  }
  libFile->IsSymlink(&isLink);
  if (isLink) {
    libFile->GetNativeTarget(aPluginFilePath);
  } else {
    libFile->GetNativePath(aPluginFilePath);
  }

  return true;
}

static bool
GetAppPaths(nsCString &aAppPath, nsCString &aAppBinaryPath)
{
  nsAutoCString appPath;
  nsAutoCString appBinaryPath(
    (CommandLine::ForCurrentProcess()->argv()[0]).c_str());

  nsAutoCString::const_iterator start, end;
  appBinaryPath.BeginReading(start);
  appBinaryPath.EndReading(end);
  if (RFindInReadable(NS_LITERAL_CSTRING(".app/Contents/MacOS/"), start, end)) {
    end = start;
    ++end; ++end; ++end; ++end;
    appBinaryPath.BeginReading(start);
    appPath.Assign(Substring(start, end));
  } else {
    return false;
  }

  nsCOMPtr<nsIFile> app, appBinary;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appPath),
                                true, getter_AddRefs(app));
  if (NS_FAILED(rv)) {
    return false;
  }
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appBinaryPath),
                       true, getter_AddRefs(appBinary));
  if (NS_FAILED(rv)) {
    return false;
  }

  bool isLink;
  app->IsSymlink(&isLink);
  if (isLink) {
    app->GetNativeTarget(aAppPath);
  } else {
    app->GetNativePath(aAppPath);
  }
  appBinary->IsSymlink(&isLink);
  if (isLink) {
    appBinary->GetNativeTarget(aAppBinaryPath);
  } else {
    appBinary->GetNativePath(aAppBinaryPath);
  }

  return true;
}

bool
GMPChild::SetMacSandboxInfo()
{
  if (!mGMPLoader) {
    return false;
  }
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    return false;
  }
  nsAutoCString appPath, appBinaryPath;
  if (!GetAppPaths(appPath, appBinaryPath)) {
    return false;
  }

  MacSandboxInfo info;
  info.type = MacSandboxType_Plugin;
  info.pluginInfo.type = MacSandboxPluginType_GMPlugin_Default;
  info.pluginInfo.pluginPath.assign(pluginDirectoryPath.get());
  info.pluginInfo.pluginBinaryPath.assign(pluginFilePath.get());
  info.appPath.assign(appPath.get());
  info.appBinaryPath.assign(appBinaryPath.get());

  mGMPLoader->SetSandboxInfo(&info);
  return true;
}
#endif // XP_MACOSX && MOZ_GMP_SANDBOX

bool
GMPChild::Init(const nsAString& aPluginPath,
               const nsAString& aVoucherPath,
               base::ProcessId aParentPid,
               MessageLoop* aIOLoop,
               IPC::Channel* aChannel)
{
  LOGD("%s pluginPath=%s", __FUNCTION__, NS_ConvertUTF16toUTF8(aPluginPath).get());

  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

#ifdef MOZ_CRASHREPORTER
  SendPCrashReporterConstructor(CrashReporter::CurrentThreadId());
#endif

  mPluginPath = aPluginPath;
  mSandboxVoucherPath = aVoucherPath;
  return true;
}

bool
GMPChild::RecvSetNodeId(const nsCString& aNodeId)
{
  LOGD("%s nodeId=%s", __FUNCTION__, aNodeId.Data());

  // Store the per origin salt for the node id. Note: we do this in a
  // separate message than RecvStartPlugin() so that the string is not
  // sitting in a string on the IPC code's call stack.
  mNodeId = aNodeId;
  return true;
}

GMPErr
GMPChild::GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI)
{
  if (!mGMPLoader) {
    return GMPGenericErr;
  }
  return mGMPLoader->GetAPI(aAPIName, aHostAPI, aPluginAPI);
}

static bool
ReadIntoArray(nsIFile* aFile,
              nsTArray<uint8_t>& aOutDst,
              size_t aMaxLength)
{
  if (!FileExists(aFile)) {
    return false;
  }

  PRFileDesc* fd = nullptr;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    return false;
  }

  int32_t length = PR_Seek(fd, 0, PR_SEEK_END);
  PR_Seek(fd, 0, PR_SEEK_SET);

  if (length < 0 || (size_t)length > aMaxLength) {
    NS_WARNING("EME file is longer than maximum allowed length");
    PR_Close(fd);
    return false;
  }
  aOutDst.SetLength(length);
  int32_t bytesRead = PR_Read(fd, aOutDst.Elements(), length);
  PR_Close(fd);
  return (bytesRead == length);
}

#ifdef XP_WIN
static bool
ReadIntoString(nsIFile* aFile,
               nsCString& aOutDst,
               size_t aMaxLength)
{
  nsTArray<uint8_t> buf;
  bool rv = ReadIntoArray(aFile, buf, aMaxLength);
  if (rv) {
    buf.AppendElement(0); // Append null terminator, required by nsC*String.
    aOutDst = nsDependentCString((const char*)buf.Elements(), buf.Length() - 1);
  }
  return rv;
}

// Pre-load DLLs that need to be used by the EME plugin but that can't be
// loaded after the sandbox has started
bool
GMPChild::PreLoadLibraries(const nsAString& aPluginPath)
{
  // This must be in sorted order and lowercase!
  static const char* whitelist[] = {
    "d3d9.dll", // Create an `IDirect3D9` to get adapter information
    "dxva2.dll", // Get monitor information
    "evr.dll", // MFGetStrideForBitmapInfoHeader
    "mfh264dec.dll", // H.264 decoder (on Windows Vista)
    "mfheaacdec.dll", // AAC decoder (on Windows Vista)
    "mfplat.dll", // MFCreateSample, MFCreateAlignedMemoryBuffer, MFCreateMediaType
    "msauddecmft.dll", // AAC decoder (on Windows 8)
    "msmpeg2adec.dll", // AAC decoder (on Windows 7)
    "msmpeg2vdec.dll", // H.264 decoder
  };

  nsCOMPtr<nsIFile> infoFile;
  GetInfoFile(aPluginPath, infoFile);

  static const size_t MAX_GMP_INFO_FILE_LENGTH = 5 * 1024;
  nsAutoCString info;
  if (!ReadIntoString(infoFile, info, MAX_GMP_INFO_FILE_LENGTH)) {
    NS_WARNING("Failed to read info file in GMP process.");
    return false;
  }

  // Note: we pass "\r\n" to SplitAt so that we'll split lines delimited
  // by \n (Unix), \r\n (Windows) and \r (old MacOSX).
  nsTArray<nsCString> lines;
  SplitAt("\r\n", info, lines);
  for (nsCString line : lines) {
    // Make lowercase.
    std::transform(line.BeginWriting(),
                   line.EndWriting(),
                   line.BeginWriting(),
                   tolower);

    const char* libraries = "libraries:";
    int32_t offset = line.Find(libraries, false, 0);
    if (offset == kNotFound) {
      continue;
    }
    // Line starts with "libraries:".
    nsTArray<nsCString> libs;
    SplitAt(",", Substring(line, offset + strlen(libraries)), libs);
    for (nsCString lib : libs) {
      lib.Trim(" ");
      for (const char* whiteListedLib : whitelist) {
        if (lib.EqualsASCII(whiteListedLib)) {
          LoadLibraryA(lib.get());
          break;
        }
      }
    }
  }

  return true;
}
#endif

bool
GMPChild::GetUTF8LibPath(nsACString& aOutLibPath)
{
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    MOZ_CRASH("Error scanning plugin path");
  }
  aOutLibPath.Assign(pluginFilePath);
  return true;
#else
  nsCOMPtr<nsIFile> libFile;
  if (!GetPluginFile(mPluginPath, libFile)) {
    return false;
  }

  if (!FileExists(libFile)) {
    NS_WARNING("Can't find GMP library file!");
    return false;
  }

  nsAutoString path;
  libFile->GetPath(path);
  aOutLibPath = NS_ConvertUTF16toUTF8(path);

  return true;
#endif
}

bool
GMPChild::AnswerStartPlugin()
{
  LOGD("%s", __FUNCTION__);

#if defined(XP_WIN)
  PreLoadLibraries(mPluginPath);
#endif
  if (!PreLoadPluginVoucher()) {
    NS_WARNING("Plugin voucher failed to load!");
    return false;
  }
  PreLoadSandboxVoucher();

  nsCString libPath;
  if (!GetUTF8LibPath(libPath)) {
    return false;
  }

  auto platformAPI = new GMPPlatformAPI();
  InitPlatformAPI(*platformAPI, this);

  mGMPLoader = GMPProcessChild::GetGMPLoader();
  if (!mGMPLoader) {
    NS_WARNING("Failed to get GMPLoader");
    delete platformAPI;
    return false;
  }

#if defined(MOZ_GMP_SANDBOX) && defined(XP_MACOSX)
  if (!SetMacSandboxInfo()) {
    NS_WARNING("Failed to set Mac GMP sandbox info");
    delete platformAPI;
    return false;
  }
#endif

  if (!mGMPLoader->Load(libPath.get(),
                        libPath.Length(),
                        mNodeId.BeginWriting(),
                        mNodeId.Length(),
                        platformAPI)) {
    NS_WARNING("Failed to load GMP");
    delete platformAPI;
    return false;
  }

  void* sh = nullptr;
  GMPAsyncShutdownHost* host = static_cast<GMPAsyncShutdownHost*>(this);
  GMPErr err = GetAPI(GMP_API_ASYNC_SHUTDOWN, host, &sh);
  if (err == GMPNoErr && sh) {
    mAsyncShutdown = reinterpret_cast<GMPAsyncShutdown*>(sh);
    SendAsyncShutdownRequired();
  }

  return true;
}

MessageLoop*
GMPChild::GMPMessageLoop()
{
  return mGMPMessageLoop;
}

void
GMPChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD("%s reason=%d", __FUNCTION__, aWhy);

  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    MOZ_ASSERT_IF(aWhy == NormalShutdown, !mGMPContentChildren[i - 1]->IsUsed());
    mGMPContentChildren[i - 1]->Close();
  }

  if (mGMPLoader) {
    mGMPLoader->Shutdown();
  }
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    _exit(0);
  }

  XRE_ShutdownChildProcess();
}

void
GMPChild::ProcessingError(Result aCode, const char* aReason)
{
  switch (aCode) {
    case MsgDropped:
      _exit(0); // Don't trigger a crash report.
    case MsgNotKnown:
      MOZ_CRASH("aborting because of MsgNotKnown");
    case MsgNotAllowed:
      MOZ_CRASH("aborting because of MsgNotAllowed");
    case MsgPayloadError:
      MOZ_CRASH("aborting because of MsgPayloadError");
    case MsgProcessingError:
      MOZ_CRASH("aborting because of MsgProcessingError");
    case MsgRouteError:
      MOZ_CRASH("aborting because of MsgRouteError");
    case MsgValueError:
      MOZ_CRASH("aborting because of MsgValueError");
    default:
      MOZ_CRASH("not reached");
  }
}

mozilla::dom::PCrashReporterChild*
GMPChild::AllocPCrashReporterChild(const NativeThreadId& aThread)
{
  return new CrashReporterChild();
}

bool
GMPChild::DeallocPCrashReporterChild(PCrashReporterChild* aCrashReporter)
{
  delete aCrashReporter;
  return true;
}

PGMPTimerChild*
GMPChild::AllocPGMPTimerChild()
{
  return new GMPTimerChild(this);
}

bool
GMPChild::DeallocPGMPTimerChild(PGMPTimerChild* aActor)
{
  MOZ_ASSERT(mTimerChild == static_cast<GMPTimerChild*>(aActor));
  mTimerChild = nullptr;
  return true;
}

GMPTimerChild*
GMPChild::GetGMPTimers()
{
  if (!mTimerChild) {
    PGMPTimerChild* sc = SendPGMPTimerConstructor();
    if (!sc) {
      return nullptr;
    }
    mTimerChild = static_cast<GMPTimerChild*>(sc);
  }
  return mTimerChild;
}

PGMPStorageChild*
GMPChild::AllocPGMPStorageChild()
{
  return new GMPStorageChild(this);
}

bool
GMPChild::DeallocPGMPStorageChild(PGMPStorageChild* aActor)
{
  mStorage = nullptr;
  return true;
}

GMPStorageChild*
GMPChild::GetGMPStorage()
{
  if (!mStorage) {
    PGMPStorageChild* sc = SendPGMPStorageConstructor();
    if (!sc) {
      return nullptr;
    }
    mStorage = static_cast<GMPStorageChild*>(sc);
  }
  return mStorage;
}

bool
GMPChild::RecvCrashPluginNow()
{
  MOZ_CRASH();
  return true;
}

bool
GMPChild::RecvBeginAsyncShutdown()
{
  LOGD("%s AsyncShutdown=%d", __FUNCTION__, mAsyncShutdown!=nullptr);

  MOZ_ASSERT(mGMPMessageLoop == MessageLoop::current());
  if (mAsyncShutdown) {
    mAsyncShutdown->BeginShutdown();
  } else {
    ShutdownComplete();
  }
  return true;
}

bool
GMPChild::RecvCloseActive()
{
  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    mGMPContentChildren[i - 1]->CloseActive();
  }
  return true;
}

void
GMPChild::ShutdownComplete()
{
  LOGD("%s", __FUNCTION__);
  MOZ_ASSERT(mGMPMessageLoop == MessageLoop::current());
  mAsyncShutdown = nullptr;
  SendAsyncShutdownComplete();
}

static void
GetPluginVoucherFile(const nsAString& aPluginPath,
                     nsCOMPtr<nsIFile>& aOutVoucherFile)
{
  nsAutoString baseName;
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  nsCOMPtr<nsIFile> libDir;
  GetFileBase(aPluginPath, aOutVoucherFile, libDir, baseName);
#else
  GetFileBase(aPluginPath, aOutVoucherFile, baseName);
#endif
  nsAutoString infoFileName = baseName + NS_LITERAL_STRING(".voucher");
  aOutVoucherFile->AppendRelativePath(infoFileName);
}

bool
GMPChild::PreLoadPluginVoucher()
{
  nsCOMPtr<nsIFile> voucherFile;
  GetPluginVoucherFile(mPluginPath, voucherFile);
  if (!FileExists(voucherFile)) {
    // Assume missing file is not fatal; that would break OpenH264.
    return true;
  }
  return ReadIntoArray(voucherFile, mPluginVoucher, MAX_VOUCHER_LENGTH);
}

void
GMPChild::PreLoadSandboxVoucher()
{
  nsCOMPtr<nsIFile> f;
  nsresult rv = NS_NewLocalFile(mSandboxVoucherPath, true, getter_AddRefs(f));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create nsIFile for sandbox voucher");
    return;
  }
  if (!FileExists(f)) {
    // Assume missing file is not fatal; that would break OpenH264.
    return;
  }

  if (!ReadIntoArray(f, mSandboxVoucher, MAX_VOUCHER_LENGTH)) {
    NS_WARNING("Failed to read sandbox voucher");
  }
}

PGMPContentChild*
GMPChild::AllocPGMPContentChild(Transport* aTransport,
                                ProcessId aOtherPid)
{
  GMPContentChild* child =
    mGMPContentChildren.AppendElement(new GMPContentChild(this))->get();
  child->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop(), ipc::ChildSide);

  return child;
}

void
GMPChild::GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild)
{
  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    UniquePtr<GMPContentChild>& toDestroy = mGMPContentChildren[i - 1];
    if (toDestroy.get() == aGMPContentChild) {
      SendPGMPContentChildDestroyed();
      MessageLoop::current()->PostTask(FROM_HERE,
                                       new DeleteTask<GMPContentChild>(toDestroy.release()));
      mGMPContentChildren.RemoveElementAt(i - 1);
      break;
    }
  }
}

} // namespace gmp
} // namespace mozilla

#undef LOG
#undef LOGD
