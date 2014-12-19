/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RefCount_h__
#define __RefCount_h__

// Note: Not thread safe!
class RefCounted {
public:
  void AddRef() {
    ++mRefCount;
  }

  void Release() {
    if (mRefCount == 1) {
      delete this;
    } else {
      --mRefCount;
    }
  }

protected:
  RefCounted()
    : mRefCount(0)
  {
  }
  virtual ~RefCounted()
  {
  }
  uint32_t mRefCount;
};

#endif // __RefCount_h__
