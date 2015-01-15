/*
 * Copyright 2013, Mozilla Foundation and contributors
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

void LOG(wchar_t* format, ...);

#ifdef LOG_SAMPLE_DECODE
#define SAMPLE_LOG LOG
#else
#define SAMPLE_LOG(...)
#endif

class IntRect {
public:
  IntRect(int32_t _x, int32_t _y, int32_t _w, int32_t _h)
    : x(_x), y(_y), width(_w), height(_h) {}
  IntRect()
    : x(0), y(0), width(0), height(0) {}
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

typedef int64_t Microseconds;

#define ENSURE(condition, ret) \
{ if (!(condition)) { LOG(L"##condition## FAILED %S:%d\n", __FILE__, __LINE__); return ret; } }

#define GMP_SUCCEEDED(x) ((x) == GMPNoErr)
#define GMP_FAILED(x) ((x) != GMPNoErr)

HRESULT
GetPictureRegion(IMFMediaType* aMediaType, IntRect& aOutPictureRegion);

HRESULT
GetDefaultStride(IMFMediaType *aType, uint32_t* aOutStride);

// Converts from microseconds to hundreds of nanoseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t
UsecsToHNs(int64_t aUsecs) {
  return aUsecs * 10;
}

// Converts from hundreds of nanoseconds to microseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t
HNsToUsecs(int64_t hNanoSecs) {
  return hNanoSecs / 10;
}

inline std::string narrow(std::wstring &wide) {
  std::string ns(wide.begin(), wide.end());
  return ns;
}

inline std::wstring widen(std::string &narrow) {
  std::wstring ws(narrow.begin(), narrow.end());
  return ws;
}

#define ARRAY_LENGTH(array_) \
  (sizeof(array_)/sizeof(array_[0]))

template<class Type>
class AutoPtr {
public:
  AutoPtr()
    : mPtr(nullptr)
  {
  }

  AutoPtr(AutoPtr<Type>& aPtr)
    : mPtr(aPtr.Forget())
  {
  }

  AutoPtr(Type* aPtr)
    : mPtr(aPtr)
  {
  }

  ~AutoPtr() {
    if (mPtr) {
      delete mPtr;
    }
  }

  Type* Forget() {
    Type* rv = mPtr;
    mPtr = nullptr;
    return rv;
  }

  AutoPtr<Type>& operator=(Type* aOther) {
    Assign(aOther);
    return *this;
  }

  AutoPtr<Type>& operator=(AutoPtr<Type>& aOther) {
    Assign(aOther.Forget());
    return *this;
  }

  Type* Get() const {
    return mPtr;
  }

  Type* operator->() const {
    assert(mPtr);
    return Get();
  }

  operator Type*() const {
    return Get();
  }

  Type** Receive() {
    return &mPtr;
  }
private:

  void Assign(Type* aPtr) {
    if (mPtr) {
      delete mPtr;
    }
    mPtr = aPtr;
  }

  Type* mPtr;
};

// Video frame microseconds are (currently) in 90kHz units, as used by RTP.
// Use this to convert to microseconds...
inline Microseconds RTPTimeToMicroseconds(int64_t rtptime) {
  return (rtptime * 1000000) / 90000;
}

inline uint32_t MicrosecondsToRTPTime(Microseconds us) {
  return uint32_t(0xffffffff & (us * 90000) / 1000000);
}

class AutoLock {
public:
  AutoLock(GMPMutex* aMutex)
    : mMutex(aMutex)
  {
    assert(aMutex);
    mMutex->Acquire();
  }
  ~AutoLock() {
    mMutex->Release();
  }
private:
  GMPMutex* mMutex;
};

void dump(const uint8_t* data, uint32_t len, const char* filename);

HRESULT
CreateMFT(const CLSID& clsid,
          const wchar_t* aDllName,
          CComPtr<IMFTransform>& aOutMFT);
