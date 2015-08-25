/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AnnexB.h"
#include "Endian.h"

#include <cstring>

using mozilla::BigEndian;

static const uint8_t kAnnexBDelimiter[] = { 0, 0, 0, 1 };

/* static */ void
AnnexB::ConvertFrameInPlace(std::vector<uint8_t>& aBuffer)
{
  for (size_t i = 0; i < aBuffer.size() - 4 - sizeof(kAnnexBDelimiter) + 1; ) {
    uint32_t nalLen = BigEndian::readUint32(&aBuffer[i]);
    memcpy(&aBuffer[i], kAnnexBDelimiter, sizeof(kAnnexBDelimiter));
    i += nalLen + 4;
  }
}

static void
ConvertParamSetToAnnexB(std::vector<uint8_t>::const_iterator& aIter,
                        size_t aCount,
                        std::vector<uint8_t>& aOutAnnexB)
{
  for (size_t i = 0; i < aCount; i++) {
    aOutAnnexB.insert(aOutAnnexB.end(), kAnnexBDelimiter,
                      kAnnexBDelimiter + sizeof(kAnnexBDelimiter));

    uint16_t len = BigEndian::readUint16(&*aIter); aIter += 2;
    aOutAnnexB.insert(aOutAnnexB.end(), aIter, aIter + len); aIter += len;
  }
}

/* static */ void
AnnexB::ConvertConfig(const std::vector<uint8_t>& aBuffer,
                      std::vector<uint8_t>& aOutAnnexB)
{
  // Skip past irrelevant headers
  auto it = aBuffer.begin() + 5;

  if (it >= aBuffer.end()) {
    return;
  }

  size_t count = *(it++) & 31;

  // Check that we have enough bytes for the Annex B conversion
  // and the next size field. Bail if not.
  if (it + count * 2 >= aBuffer.end()) {
    return;
  }

  ConvertParamSetToAnnexB(it, count, aOutAnnexB);

  // Check that we have enough bytes for the Annex B conversion.
  count = *(it++);
  if (it + count * 2 > aBuffer.end()) {
    aOutAnnexB.clear();
    return;
  }

  ConvertParamSetToAnnexB(it, count, aOutAnnexB);
}
