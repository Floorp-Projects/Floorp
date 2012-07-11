/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
    notes.Init(4);
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
