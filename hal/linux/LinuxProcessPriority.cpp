/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"

#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

#include <fcntl.h>
#include <unistd.h>

using namespace mozilla::hal;

namespace mozilla::hal_impl {

/* The Linux OOM score is a value computed by the kernel ranging between 0 and
 * 1000 and indicating how likely is a process to be killed when memory is
 * tight. The larger the value the more likely a process is to be killed. The
 * score is computed based on the amount of memory used plus an adjustment
 * value. We chose our adjustment values to make it likely that processes are
 * killed in the following order:
 *
 * 1. preallocated processes
 * 2. background processes
 * 3. background processes playing video (but no audio)
 * 4. foreground processes or processes playing or recording audio
 * 5. the parent process
 *
 * At the time of writing (2022) the base score for a process consuming very
 * little memory seems to be around ~667. Our adjustments are thus designed
 * to ensure this order starting from a 667 baseline but trying not to go too
 * close to the 1000 limit where they would be clamped. */

const uint32_t kParentOomScoreAdjust = 0;
const uint32_t kForegroundOomScoreAdjust = 100;
const uint32_t kBackgroundPerceivableOomScoreAdjust = 133;
const uint32_t kBackgroundOomScoreAdjust = 167;
const uint32_t kPreallocOomScoreAdjust = 233;

static uint32_t OomScoreAdjForPriority(ProcessPriority aPriority) {
  switch (aPriority) {
    case PROCESS_PRIORITY_BACKGROUND:
      return kBackgroundOomScoreAdjust;
    case PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
      return kBackgroundPerceivableOomScoreAdjust;
    case PROCESS_PRIORITY_PREALLOC:
      return kPreallocOomScoreAdjust;
    case PROCESS_PRIORITY_FOREGROUND:
      return kForegroundOomScoreAdjust;
    default:
      return kParentOomScoreAdjust;
  }
}

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  HAL_LOG("LinuxProcessPriority - SetProcessPriority(%d, %s)\n", aPid,
          ProcessPriorityToString(aPriority));

  uint32_t oomScoreAdj = OomScoreAdjForPriority(aPriority);

  char path[32] = {};
  SprintfLiteral(path, "/proc/%d/oom_score_adj", aPid);

  char oomScoreAdjStr[11] = {};
  SprintfLiteral(oomScoreAdjStr, "%d", oomScoreAdj);

  int fd = open(path, O_WRONLY);
  if (fd < 0) {
    return;
  }
  const size_t len = strlen(oomScoreAdjStr);
  Unused << write(fd, oomScoreAdjStr, len);
  Unused << close(fd);
}

}  // namespace mozilla::hal_impl
