/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A handy class that will allocate data for size*T objects on the stack and
 * otherwise allocate them on the heap. It is similar in purpose to AutoTArray */

template <class T, size_t size>
class StackArray
{
public:
  StackArray(size_t count) {
    if (count > size) {
      mData = new T[count];
    } else {
      mData = mStackData;
    }
  }
  ~StackArray() {
    if (mData != mStackData) {
      delete[] mData;
    }
  }
  T& operator[](size_t n) { return mData[n]; }
  const T& operator[](size_t n) const { return mData[n]; }
  T* data() { return mData; };
private:
  T mStackData[size];
  T* mData;
};
