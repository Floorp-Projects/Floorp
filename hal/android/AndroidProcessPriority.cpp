/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "mozilla/java/GeckoProcessManagerWrappers.h"
#include "mozilla/java/GeckoProcessTypeWrappers.h"
#include "mozilla/java/ServiceAllocatorWrappers.h"

/**
 * Bucket the Gecko HAL process priority level into one of the three Android
 * priority levels.
 */
static mozilla::java::ServiceAllocator::PriorityLevel::LocalRef
ToJavaPriorityLevel(const ProcessPriority aPriority) {
  if (aPriority >= PROCESS_PRIORITY_FOREGROUND) {
    return mozilla::java::ServiceAllocator::PriorityLevel::FOREGROUND();
  } else if (aPriority <= PROCESS_PRIORITY_PREALLOC &&
             aPriority >= PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
    return mozilla::java::ServiceAllocator::PriorityLevel::BACKGROUND();
  }

  return mozilla::java::ServiceAllocator::PriorityLevel::IDLE();
}

namespace mozilla {
namespace hal_impl {

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  if (aPriority == PROCESS_PRIORITY_PARENT_PROCESS) {
    // This is the parent process itself, which we do not control.
    return;
  }

  const int32_t intPriority = static_cast<int32_t>(aPriority);
  if (intPriority < 0 || intPriority >= NUM_PROCESS_PRIORITY) {
    return;
  }

  auto contentProcType = java::GeckoProcessType::CONTENT();
  auto selector =
      java::GeckoProcessManager::Selector::New(contentProcType, aPid);
  auto priorityLevel = ToJavaPriorityLevel(aPriority);

  // To Android, a lower-valued integer is a higher relative priority.
  // We take the integer value of the enum and subtract it from the value
  // of the highest Gecko priority level to obtain a 0-based indicator of
  // the relative priority within the Java PriorityLevel.
  const int32_t relativeImportance =
      (static_cast<int32_t>(NUM_PROCESS_PRIORITY) - 1) - intPriority;

  java::GeckoProcessManager::SetProcessPriority(selector, priorityLevel,
                                                relativeImportance);
}

}  // namespace hal_impl
}  // namespace mozilla
