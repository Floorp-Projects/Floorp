/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "CrashReporterParent.h"

#include "base/process_util.h"

#include <time.h>

using namespace base;

namespace mozilla {
namespace dom {

void
CrashReporterParent::ActorDestroy(ActorDestroyReason why)
{
#if defined(__ANDROID__) && defined(MOZ_CRASHREPORTER)
  CrashReporter::RemoveLibraryMappingsForChild(ProcessId(OtherProcess()));
#endif
}

bool
CrashReporterParent::RecvAddLibraryMappings(const InfallibleTArray<Mapping>& mappings)
{
#if defined(__ANDROID__) && defined(MOZ_CRASHREPORTER)
  for (PRUint32 i = 0; i < mappings.Length(); i++) {
    const Mapping& m = mappings[i];
    CrashReporter::AddLibraryMappingForChild(ProcessId(OtherProcess()),
                                             m.library_name().get(),
                                             m.file_id().get(),
                                             m.start_address(),
                                             m.mapping_length(),
                                             m.file_offset());
  }
#endif
  return true;
}

bool
CrashReporterParent::RecvAnnotateCrashReport(const nsCString& key,
                                             const nsCString& data)
{
#ifdef MOZ_CRASHREPORTER
    mNotes.Put(key, data);
#endif
    return true;
}

bool
CrashReporterParent::RecvAppendAppNotes(const nsCString& data)
{
    mAppNotes.Append(data);
    return true;
}

CrashReporterParent::CrashReporterParent()
: mStartTime(time(NULL))
, mInitialized(false)
{
    MOZ_COUNT_CTOR(CrashReporterParent);

#ifdef MOZ_CRASHREPORTER
    mNotes.Init(4);
#endif
}

CrashReporterParent::~CrashReporterParent()
{
    MOZ_COUNT_DTOR(CrashReporterParent);
}

void
CrashReporterParent::SetChildData(const NativeThreadId& tid,
                                  const PRUint32& processType)
{
    mInitialized = true;
    mMainThread = tid;
    mProcessType = processType;
}

#ifdef MOZ_CRASHREPORTER
bool
CrashReporterParent::GenerateHangCrashReport(const AnnotationTable* processNotes)
{
    if (mChildDumpID.IsEmpty())
        return false;

    GenerateChildData(processNotes);

    CrashReporter::AnnotationTable notes;
    if (!notes.Init(4))
        return false;
    notes.Put(nsDependentCString("HangID"), NS_ConvertUTF16toUTF8(mHangID));
    if (!CrashReporter::AppendExtraData(mParentDumpID, notes))
        NS_WARNING("problem appending parent data to .extra");
    return true;
}

bool
CrashReporterParent::GenerateChildData(const AnnotationTable* processNotes)
{
    MOZ_ASSERT(mInitialized);

    nsCAutoString type;
    switch (mProcessType) {
        case GeckoProcessType_Content:
            type = NS_LITERAL_CSTRING("content");
            break;
        case GeckoProcessType_Plugin:
            type = NS_LITERAL_CSTRING("plugin");
            break;
        default:
            NS_ERROR("unknown process type");
            break;
    }
    mNotes.Put(NS_LITERAL_CSTRING("ProcessType"), type);

    char startTime[32];
    sprintf(startTime, "%lld", static_cast<PRInt64>(mStartTime));
    mNotes.Put(NS_LITERAL_CSTRING("StartupTime"), nsDependentCString(startTime));

    if (!mAppNotes.IsEmpty())
        mNotes.Put(NS_LITERAL_CSTRING("Notes"), mAppNotes);

    bool ret = CrashReporter::AppendExtraData(mChildDumpID, mNotes);
    if (ret && processNotes)
        ret = CrashReporter::AppendExtraData(mChildDumpID, *processNotes);
    if (!ret)
        NS_WARNING("problem appending child data to .extra");
    return ret;
}
#endif

} // namespace dom
} // namespace mozilla
