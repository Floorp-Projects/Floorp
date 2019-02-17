/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureSync.h"

#include <unordered_set>

#include "base/process_util.h"
#include "chrome/common/mach_ipc_mac.h"
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/StaticMonitor.h"
#include "mozilla/StaticPtr.h"

#ifdef DEBUG
#  define LOG_ERROR(str, args...)                                  \
    PR_BEGIN_MACRO                                                 \
    mozilla::SmprintfPointer msg = mozilla::Smprintf(str, ##args); \
    NS_WARNING(msg.get());                                         \
    PR_END_MACRO
#else
#  define LOG_ERROR(str, args...) \
    do { /* nothing */            \
    } while (0)
#endif

namespace mozilla {

namespace layers {

// Hold raw pointers and trust that TextureSourceProviders will be
// unregistered in their destructors - we don't want to keep these
// alive, and destroying them from the main thread will be an
// error anyway.
StaticAutoPtr<nsTArray<TextureSourceProvider*>> gTextureSourceProviders;

static std::map<pid_t, std::unordered_set<uint64_t>> gProcessTextureIds;
static StaticMonitor gTextureLockMonitor;

const int kSendMessageTimeout = 1000;
const int kTextureLockTimeout = 32;  // We really don't want to wait more than
                                     // two frames for a texture to unlock. This
                                     // will in any case be very uncommon.

struct WaitForTexturesReply {
  bool success;
};

struct WaitForTexturesRequest {
  pid_t pid;
};

static std::unordered_set<uint64_t>* GetLockedTextureIdsForProcess(pid_t pid) {
  gTextureLockMonitor.AssertCurrentThreadOwns();

  if (gProcessTextureIds.find(pid) == gProcessTextureIds.end()) {
    gProcessTextureIds[pid] = std::unordered_set<uint64_t>();
  }

  return &gProcessTextureIds.at(pid);
}

static bool WaitForTextureIdsToUnlock(pid_t pid,
                                      const Span<const uint64_t>& textureIds) {
  {
    StaticMonitorAutoLock lock(gTextureLockMonitor);
    std::unordered_set<uint64_t>* freedTextureIds =
        GetLockedTextureIdsForProcess(pid);

    TimeStamp start = TimeStamp::Now();
    while (true) {
      bool allCleared = true;
      for (uint64_t textureId : textureIds) {
        if (freedTextureIds->find(textureId) != freedTextureIds->end()) {
          allCleared = false;
        }
      }

      if (allCleared) {
        return true;
      }

      if (lock.Wait(TimeDuration::FromMilliseconds(kTextureLockTimeout)) ==
          CVStatus::Timeout) {
        return false;
      }

      // In case the monitor gets signaled multiple times, each less than
      // kTextureLockTimeout.  This ensures that the total time we wait is
      // < 2 * kTextureLockTimeout
      if ((TimeStamp::Now() - start).ToMilliseconds() >
          (double)kTextureLockTimeout) {
        return false;
      }
    }
  }
}

static void CheckTexturesForUnlock() {
  if (gTextureSourceProviders) {
    for (auto it = gTextureSourceProviders->begin();
         it != gTextureSourceProviders->end(); ++it) {
      (*it)->TryUnlockTextures();
    }
  }
}

void TextureSync::DispatchCheckTexturesForUnlock() {
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("CheckTexturesForUnlock", &CheckTexturesForUnlock);
  CompositorThreadHolder::Loop()->PostTask(task.forget());
}

void TextureSync::HandleWaitForTexturesMessage(MachReceiveMessage* rmsg,
                                               ipc::MemoryPorts* ports) {
  WaitForTexturesRequest* req =
      reinterpret_cast<WaitForTexturesRequest*>(rmsg->GetData());
  uint64_t* textureIds = (uint64_t*)(req + 1);
  uint32_t textureIdsLength =
      (rmsg->GetDataLength() - sizeof(WaitForTexturesRequest)) /
      sizeof(uint64_t);

  bool success = WaitForTextureIdsToUnlock(
      req->pid, MakeSpan<uint64_t>(textureIds, textureIdsLength));

  if (!success) {
    LOG_ERROR("Waiting for textures to unlock failed.\n");
  }

  MachSendMessage msg(ipc::kReturnWaitForTexturesMsg);
  WaitForTexturesReply replydata;
  replydata.success = success;
  msg.SetData(&replydata, sizeof(WaitForTexturesReply));
  kern_return_t err = ports->mSender->SendMessage(msg, kSendMessageTimeout);
  if (KERN_SUCCESS != err) {
    LOG_ERROR("SendMessage failed 0x%x %s\n", err, mach_error_string(err));
  }
}

void TextureSync::RegisterTextureSourceProvider(
    TextureSourceProvider* textureSourceProvider) {
  if (!gTextureSourceProviders) {
    gTextureSourceProviders = new nsTArray<TextureSourceProvider*>();
  }
  MOZ_RELEASE_ASSERT(!gTextureSourceProviders->Contains(textureSourceProvider));
  gTextureSourceProviders->AppendElement(textureSourceProvider);
}

void TextureSync::UnregisterTextureSourceProvider(
    TextureSourceProvider* textureSourceProvider) {
  if (gTextureSourceProviders) {
    MOZ_ASSERT(gTextureSourceProviders->Contains(textureSourceProvider));
    gTextureSourceProviders->RemoveElement(textureSourceProvider);
    if (gTextureSourceProviders->Length() == 0) {
      gTextureSourceProviders = nullptr;
    }
  }
}

void TextureSync::SetTexturesLocked(pid_t pid,
                                    const nsTArray<uint64_t>& textureIds) {
  StaticMonitorAutoLock mal(gTextureLockMonitor);
  std::unordered_set<uint64_t>* lockedTextureIds =
      GetLockedTextureIdsForProcess(pid);
  for (uint64_t textureId : textureIds) {
    lockedTextureIds->insert(textureId);
  }
}

void TextureSync::SetTexturesUnlocked(pid_t pid,
                                      const nsTArray<uint64_t>& textureIds) {
  bool oneErased = false;
  {
    StaticMonitorAutoLock mal(gTextureLockMonitor);
    std::unordered_set<uint64_t>* lockedTextureIds =
        GetLockedTextureIdsForProcess(pid);
    for (uint64_t textureId : textureIds) {
      if (lockedTextureIds->erase(textureId)) {
        oneErased = true;
      }
    }
  }
  if (oneErased) {
    gTextureLockMonitor.NotifyAll();
  }
}

void TextureSync::Shutdown() {
  {
    StaticMonitorAutoLock lock(gTextureLockMonitor);

    for (auto& lockedTextureIds : gProcessTextureIds) {
      lockedTextureIds.second.clear();
    }
  }

  gTextureLockMonitor.NotifyAll();

  {
    StaticMonitorAutoLock lock(gTextureLockMonitor);
    gProcessTextureIds.clear();
  }
}

void TextureSync::UpdateTextureLocks(base::ProcessId aProcessId) {
  if (aProcessId == base::GetCurrentProcId()) {
    DispatchCheckTexturesForUnlock();
    return;
  }

  MachSendMessage smsg(ipc::kUpdateTextureLocksMsg);
  smsg.SetData(&aProcessId, sizeof(aProcessId));
  ipc::SharedMemoryBasic::SendMachMessage(aProcessId, smsg, NULL);
}

bool TextureSync::WaitForTextures(base::ProcessId aProcessId,
                                  const nsTArray<uint64_t>& textureIds) {
  if (aProcessId == base::GetCurrentProcId()) {
    bool success =
        WaitForTextureIdsToUnlock(aProcessId, MakeSpan<uint64_t>(textureIds));
    if (!success) {
      LOG_ERROR("Failed waiting for textures to unlock.\n");
    }

    return success;
  }

  MachSendMessage smsg(ipc::kWaitForTexturesMsg);
  size_t messageSize =
      sizeof(WaitForTexturesRequest) + textureIds.Length() * sizeof(uint64_t);
  UniquePtr<uint8_t[]> messageData = MakeUnique<uint8_t[]>(messageSize);
  WaitForTexturesRequest* req = (WaitForTexturesRequest*)messageData.get();
  uint64_t* reqTextureIds = (uint64_t*)(req + 1);

  for (uint32_t i = 0; i < textureIds.Length(); ++i) {
    reqTextureIds[i] = textureIds[i];
  }

  req->pid = base::GetCurrentProcId();
  bool dataWasSet = smsg.SetData(req, messageSize);

  if (!dataWasSet) {
    LOG_ERROR("Data was too large: %zu\n", messageSize);
    return false;
  }

  MachReceiveMessage msg;
  bool success =
      ipc::SharedMemoryBasic::SendMachMessage(aProcessId, smsg, &msg);
  if (!success) {
    return false;
  }

  if (msg.GetDataLength() != sizeof(WaitForTexturesReply)) {
    LOG_ERROR("Improperly formatted reply\n");
    return false;
  }

  WaitForTexturesReply* msg_data =
      reinterpret_cast<WaitForTexturesReply*>(msg.GetData());
  if (!msg_data->success) {
    LOG_ERROR("Failed waiting for textures to unlock.\n");
    return false;
  }

  return true;
}

void TextureSync::CleanupForPid(base::ProcessId aProcessId) {
  {
    StaticMonitorAutoLock lock(gTextureLockMonitor);
    std::unordered_set<uint64_t>* lockedTextureIds =
        GetLockedTextureIdsForProcess(aProcessId);
    lockedTextureIds->clear();
  }
  gTextureLockMonitor.NotifyAll();
}

}  // namespace layers

}  // namespace mozilla
