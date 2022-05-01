/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollGeneration_h_
#define mozilla_ScrollGeneration_h_

#include <cstdint>
#include <iosfwd>

namespace mozilla {

struct ScrollGenerationCounter;

class APZTag {};
class MainThreadTag {};

template <typename Tag>
struct ScrollGeneration;

template <typename Tag>
std::ostream& operator<<(std::ostream& aStream,
                         const ScrollGeneration<Tag>& aGen);

template <typename Tag>
struct ScrollGeneration {
  friend struct ScrollGenerationCounter;

 private:
  // Private constructor; use ScrollGenerationCounter to get a new instance.
  explicit ScrollGeneration(uint64_t aValue);

 public:
  // Dummy constructor, needed for IPDL purposes. Not intended for manual use.
  ScrollGeneration();

  uint64_t Raw() const { return mValue; }

  bool operator<(const ScrollGeneration<Tag>& aOther) const;
  bool operator==(const ScrollGeneration<Tag>& aOther) const;
  bool operator!=(const ScrollGeneration<Tag>& aOther) const;

  friend std::ostream& operator<< <>(std::ostream& aStream,
                                     const ScrollGeneration<Tag>& aGen);

 private:
  uint64_t mValue;
};

using APZScrollGeneration = ScrollGeneration<APZTag>;
using MainThreadScrollGeneration = ScrollGeneration<MainThreadTag>;

struct ScrollGenerationCounter {
  MainThreadScrollGeneration NewMainThreadGeneration() {
    uint64_t value = ++mCounter;
    return MainThreadScrollGeneration(value);
  }

  APZScrollGeneration NewAPZGeneration() {
    uint64_t value = ++mCounter;
    return APZScrollGeneration(value);
  }

 private:
  uint64_t mCounter = 0;
};

}  // namespace mozilla

#endif  // mozilla_ScrollGeneration_h_
