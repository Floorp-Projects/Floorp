/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollGeneration_h_
#define mozilla_ScrollGeneration_h_

#include <cstdint>
#include <iosfwd>

namespace mozilla {

struct ScrollGeneration {
 private:
  // Private constructor; use New() to get a new instance.
  explicit ScrollGeneration(uint64_t aValue);

 public:
  // Dummy constructor, needed for IPDL purposes. Not intended for manual use.
  ScrollGeneration();

  // Returns a new ScrollGeneration with a unique value.
  static ScrollGeneration New();

  bool operator<(const ScrollGeneration& aOther) const;
  bool operator==(const ScrollGeneration& aOther) const;
  bool operator!=(const ScrollGeneration& aOther) const;

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ScrollGeneration& aGen);

 private:
  static uint64_t sCounter;
  uint64_t mValue;
};

}  // namespace mozilla

#endif  // mozilla_ScrollGeneration_h_
