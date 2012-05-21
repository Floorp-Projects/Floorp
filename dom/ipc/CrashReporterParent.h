/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PCrashReporterParent.h"
#include "mozilla/dom/TabMessageUtils.h"
#include "nsXULAppAPI.h"
#include "nsILocalFile.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
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

  /* Attempt to create a bare-bones crash report for a hang, along with extra
     process-specific annotations present in the given AnnotationTable. Returns
     true if successful, false otherwise.
  */
  bool
  GenerateHangCrashReport(const AnnotationTable* processNotes);

  /* Attempt to create a bare-bones crash report, along with extra process-
     specific annotations present in the given AnnotationTable. Returns true if
     successful, false otherwise.
  */
  template<class Toplevel>
  bool
  GenerateCrashReport(Toplevel* t, const AnnotationTable* processNotes);

  /* Instantiate a new crash reporter actor from a given parent that manages
     the protocol.
  */
  template<class Toplevel>
  static bool CreateCrashReporter(Toplevel* actor);
#endif
  /* Initialize this reporter with data from the child process */
  void
    SetChildData(const NativeThreadId& id, const PRUint32& processType);

  /* Returns the shared hang ID of a parent/child paired minidump.
     GeneratePairedMinidump must be called first.
  */
  const nsString& HangID() {
    return mHangID;
  }
  /* Returns the ID of the parent minidump.
     GeneratePairedMinidump must be called first.
  */
  const nsString& ParentDumpID() {
    return mParentDumpID;
  }
  /* Returns the ID of the child minidump.
     GeneratePairedMinidump or GenerateCrashReport must be called first.
  */
  const nsString& ChildDumpID() {
    return mChildDumpID;
  }

 protected:
  virtual void ActorDestroy(ActorDestroyReason why);

  virtual bool
    RecvAddLibraryMappings(const InfallibleTArray<Mapping>& m);
  virtual bool
    RecvAnnotateCrashReport(const nsCString& key, const nsCString& data);
  virtual bool
    RecvAppendAppNotes(const nsCString& data);

#ifdef MOZ_CRASHREPORTER
  bool
  GenerateChildData(const AnnotationTable* processNotes);

  AnnotationTable mNotes;
#endif
  nsCString mAppNotes;
  nsString mHangID;
  nsString mChildDumpID;
  nsString mParentDumpID;
  NativeThreadId mMainThread;
  time_t mStartTime;
  PRUint32 mProcessType;
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
  nsCOMPtr<nsILocalFile> childDump;
  nsCOMPtr<nsILocalFile> parentDump;
  if (CrashReporter::CreatePairedMinidumps(child,
                                           mMainThread,
                                           &mHangID,
                                           getter_AddRefs(childDump),
                                           getter_AddRefs(parentDump)) &&
      CrashReporter::GetIDFromMinidump(childDump, mChildDumpID) &&
      CrashReporter::GetIDFromMinidump(parentDump, mParentDumpID)) {
    return true;
  }
  return false;
}

template<class Toplevel>
inline bool
CrashReporterParent::GenerateCrashReport(Toplevel* t,
                                         const AnnotationTable* processNotes)
{
  nsCOMPtr<nsILocalFile> crashDump;
  if (t->TakeMinidump(getter_AddRefs(crashDump)) &&
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
  PRUint32 processType;
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
