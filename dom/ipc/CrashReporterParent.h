/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CrashReporterParent_h
#define mozilla_dom_CrashReporterParent_h

#include "mozilla/dom/PCrashReporterParent.h"
#include "mozilla/dom/TabMessageUtils.h"
#include "nsIFile.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsDataHashtable.h"
#endif

namespace mozilla {
namespace dom {
class ProcessReporter;

class CrashReporterParent :
    public PCrashReporterParent
{
#ifdef MOZ_CRASHREPORTER
  typedef CrashReporter::AnnotationTable AnnotationTable;
#endif
public:
  CrashReporterParent();
  virtual ~CrashReporterParent();

#ifdef MOZ_CRASHREPORTER
  /* Attempt to generate a parent/child pair of minidumps from the given
     toplevel actor in the event of a hang. Returns true if successful,
     false otherwise.
  */
  template<class Toplevel>
  bool
  GeneratePairedMinidump(Toplevel* t);

  /* Attempt to create a bare-bones crash report, along with extra process-
     specific annotations present in the given AnnotationTable. Returns true if
     successful, false otherwise.
  */
  template<class Toplevel>
  bool
  GenerateCrashReport(Toplevel* t, const AnnotationTable* processNotes);

  /**
   * Add the .extra data for an existing crash report.
   */
  bool
  GenerateChildData(const AnnotationTable* processNotes);

  bool
  GenerateCrashReportForMinidump(nsIFile* minidump,
                                 const AnnotationTable* processNotes);

  /* Instantiate a new crash reporter actor from a given parent that manages
     the protocol.
  */
  template<class Toplevel>
  static bool CreateCrashReporter(Toplevel* actor);
#endif
  /* Initialize this reporter with data from the child process */
  void
    SetChildData(const NativeThreadId& id, const uint32_t& processType);

  /* Returns the ID of the child minidump.
     GeneratePairedMinidump or GenerateCrashReport must be called first.
  */
  const nsString& ChildDumpID() {
    return mChildDumpID;
  }

  void
  AnnotateCrashReport(const nsCString& key, const nsCString& data);

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
    RecvAnnotateCrashReport(const nsCString& key, const nsCString& data) MOZ_OVERRIDE {
    AnnotateCrashReport(key, data);
    return true;
  }
  virtual bool
    RecvAppendAppNotes(const nsCString& data) MOZ_OVERRIDE;
  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext *aCtx) MOZ_OVERRIDE;

#ifdef MOZ_CRASHREPORTER
  void
  NotifyCrashService();
#endif

#ifdef MOZ_CRASHREPORTER
  AnnotationTable mNotes;
#endif
  nsCString mAppNotes;
  nsString mChildDumpID;
  NativeThreadId mMainThread;
  time_t mStartTime;
  uint32_t mProcessType;
  bool mInitialized;
};

#ifdef MOZ_CRASHREPORTER
template<class Toplevel>
inline bool
CrashReporterParent::GeneratePairedMinidump(Toplevel* t)
{
  CrashReporter::ProcessHandle child;
#ifdef XP_MACOSX
  child = t->Process()->GetChildTask();
#else
  child = t->OtherProcess();
#endif
  nsCOMPtr<nsIFile> childDump;
  if (CrashReporter::CreatePairedMinidumps(child,
                                           mMainThread,
                                           getter_AddRefs(childDump)) &&
      CrashReporter::GetIDFromMinidump(childDump, mChildDumpID)) {
    return true;
  }
  return false;
}

template<class Toplevel>
inline bool
CrashReporterParent::GenerateCrashReport(Toplevel* t,
                                         const AnnotationTable* processNotes)
{
  nsCOMPtr<nsIFile> crashDump;
  if (t->TakeMinidump(getter_AddRefs(crashDump), nullptr) &&
      CrashReporter::GetIDFromMinidump(crashDump, mChildDumpID)) {
    return GenerateChildData(processNotes);
  }
  return false;
}

template<class Toplevel>
/* static */ bool
CrashReporterParent::CreateCrashReporter(Toplevel* actor)
{
#ifdef MOZ_CRASHREPORTER
  NativeThreadId id;
  uint32_t processType;
  PCrashReporterParent* p =
      actor->CallPCrashReporterConstructor(&id, &processType);
  if (p) {
    static_cast<CrashReporterParent*>(p)->SetChildData(id, processType);
  } else {
    NS_ERROR("Error creating crash reporter actor");
  }
  return !!p;
#endif
  return false;
}

#endif

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CrashReporterParent_h
