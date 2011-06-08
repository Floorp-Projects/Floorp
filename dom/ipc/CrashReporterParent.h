/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
  static void CreateCrashReporter(Toplevel* actor);
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
/* static */ void
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
#endif
}

#endif

} // namespace dom
} // namespace mozilla
