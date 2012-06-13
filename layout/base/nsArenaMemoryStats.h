/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArenaMemoryStats_h
#define nsArenaMemoryStats_h

#define FRAME_ID_STAT_FIELD(classname) mArena##classname

struct nsArenaMemoryStats {
#define FRAME_ID(classname) size_t FRAME_ID_STAT_FIELD(classname);
#include "nsFrameIdList.h"
#undef FRAME_ID
  size_t mLineBoxes;
  size_t mRuleNodes;
  size_t mStyleContexts;
  size_t mOther;
};

#endif // nsArenaMemoryStats_h
