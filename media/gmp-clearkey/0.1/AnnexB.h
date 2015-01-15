/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AnnexB_h__
#define __AnnexB_h__

#include <cstdint>
#include <vector>

class AnnexB
{
public:
  static void ConvertFrameInPlace(std::vector<uint8_t>& aBuffer);

  static void ConvertConfig(const std::vector<uint8_t>& aBuffer,
                            std::vector<uint8_t>& aOutAnnexB);
};

#endif // __AnnexB_h__
