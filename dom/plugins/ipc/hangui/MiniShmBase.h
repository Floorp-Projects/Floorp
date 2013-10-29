/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_MiniShmBase_h
#define mozilla_plugins_MiniShmBase_h

#include "base/basictypes.h"

#include "nsDebug.h"

#include <windows.h>

namespace mozilla {
namespace plugins {

/**
 * This class is used to provide RAII semantics for mapped views.
 * @see ScopedHandle
 */
class ScopedMappedFileView
{
public:
  explicit
  ScopedMappedFileView(LPVOID aView)
    : mView(aView)
  {
  }

  ~ScopedMappedFileView()
  {
    Close();
  }

  void
  Close()
  {
    if (mView) {
      ::UnmapViewOfFile(mView);
      mView = nullptr;
    }
  }

  void
  Set(LPVOID aView)
  {
    Close();
    mView = aView;
  }

  LPVOID
  Get() const
  {
    return mView;
  }

  LPVOID
  Take()
  {
    LPVOID result = mView;
    mView = nullptr;
    return result;
  }

  operator LPVOID()
  {
    return mView;
  }

  bool
  IsValid() const
  {
    return (mView);
  }

private:
  DISALLOW_COPY_AND_ASSIGN(ScopedMappedFileView);

  LPVOID mView;
};

class MiniShmBase;

class MiniShmObserver
{
public:
  /**
   * This function is called whenever there is a new shared memory request.
   * @param aMiniShmObj MiniShmBase object that may be used to read and 
   *                    write from shared memory.
   */
  virtual void OnMiniShmEvent(MiniShmBase *aMiniShmObj) = 0;
  /**
   * This function is called once when a MiniShmParent and a MiniShmChild
   * object have successfully negotiated a connection.
   *
   * @param aMiniShmObj MiniShmBase object that may be used to read and 
   *                    write from shared memory.
   */
  virtual void OnMiniShmConnect(MiniShmBase *aMiniShmObj) { }
};

/**
 * Base class for MiniShm connections. This class defines the common 
 * interfaces and code between parent and child.
 */
class MiniShmBase
{
public:
  /**
   * Obtains a writable pointer into shared memory of type T.
   * typename T must be plain-old-data and contain an unsigned integral 
   * member T::identifier that uniquely identifies T with respect to 
   * other types used by the protocol being implemented.
   *
   * @param aPtr Pointer to receive the shared memory address.
   *             This value is set if and only if the function 
   *             succeeded.
   * @return NS_OK if and only if aPtr was successfully obtained.
   *         NS_ERROR_ILLEGAL_VALUE if type T is not valid for MiniShm.
   *         NS_ERROR_NOT_INITIALIZED if there is no valid MiniShm connection.
   *         NS_ERROR_NOT_AVAILABLE if the memory is not safe to write.
   */
  template<typename T> nsresult
  GetWritePtr(T*& aPtr)
  {
    if (!mWriteHeader || !mGuard) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    if (sizeof(T) > mPayloadMaxLen ||
        T::identifier <= RESERVED_CODE_LAST) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    if (::WaitForSingleObject(mGuard, mTimeout) != WAIT_OBJECT_0) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    mWriteHeader->mId = T::identifier;
    mWriteHeader->mPayloadLen = sizeof(T);
    aPtr = reinterpret_cast<T*>(mWriteHeader + 1);
    return NS_OK;
  }

  /**
   * Obtains a readable pointer into shared memory of type T.
   * typename T must be plain-old-data and contain an unsigned integral 
   * member T::identifier that uniquely identifies T with respect to 
   * other types used by the protocol being implemented.
   *
   * @param aPtr Pointer to receive the shared memory address.
   *             This value is set if and only if the function 
   *             succeeded.
   * @return NS_OK if and only if aPtr was successfully obtained.
   *         NS_ERROR_ILLEGAL_VALUE if type T is not valid for MiniShm or if
   *                                type T does not match the type of the data
   *                                stored in shared memory.
   *         NS_ERROR_NOT_INITIALIZED if there is no valid MiniShm connection.
   */
  template<typename T> nsresult
  GetReadPtr(const T*& aPtr)
  {
    if (!mReadHeader) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    if (mReadHeader->mId != T::identifier ||
        sizeof(T) != mReadHeader->mPayloadLen) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    aPtr = reinterpret_cast<const T*>(mReadHeader + 1);
    return NS_OK;
  }

  /**
   * Fires the peer's event causing its request handler to execute.
   *
   * @return Should return NS_OK if the send was successful.
   */
  virtual nsresult
  Send() = 0;

protected:
  /**
   * MiniShm reserves some identifier codes for its own use. Any 
   * identifiers used by MiniShm protocol implementations must be 
   * greater than RESERVED_CODE_LAST.
   */
  enum ReservedCodes
  {
    RESERVED_CODE_INIT = 0,
    RESERVED_CODE_INIT_COMPLETE = 1,
    RESERVED_CODE_LAST = RESERVED_CODE_INIT_COMPLETE
  };

  struct MiniShmHeader
  {
    unsigned int  mId;
    unsigned int  mPayloadLen;
  };

  struct MiniShmInit
  {
    enum identifier_t
    {
      identifier = RESERVED_CODE_INIT
    };
    HANDLE    mParentEvent;
    HANDLE    mParentGuard;
    HANDLE    mChildEvent;
    HANDLE    mChildGuard;
  };

  struct MiniShmInitComplete
  {
    enum identifier_t
    {
      identifier = RESERVED_CODE_INIT_COMPLETE
    };
    bool      mSucceeded;
  };

  MiniShmBase()
    : mObserver(nullptr),
      mWriteHeader(nullptr),
      mReadHeader(nullptr),
      mPayloadMaxLen(0),
      mGuard(nullptr),
      mTimeout(INFINITE)
  {
  }
  virtual ~MiniShmBase()
  { }

  virtual void
  OnEvent()
  {
    if (mObserver) {
      mObserver->OnMiniShmEvent(this);
    }
  }

  virtual void
  OnConnect()
  {
    if (mObserver) {
      mObserver->OnMiniShmConnect(this);
    }
  }

  nsresult
  SetView(LPVOID aView, const unsigned int aSize, bool aIsChild)
  {
    if (!aView || aSize <= 2 * sizeof(MiniShmHeader)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    // Divide the region into halves for parent and child
    if (aIsChild) {
      mReadHeader = static_cast<MiniShmHeader*>(aView);
      mWriteHeader = reinterpret_cast<MiniShmHeader*>(static_cast<char*>(aView)
                                                      + aSize / 2U);
    } else {
      mWriteHeader = static_cast<MiniShmHeader*>(aView);
      mReadHeader = reinterpret_cast<MiniShmHeader*>(static_cast<char*>(aView)
                                                     + aSize / 2U);
    }
    mPayloadMaxLen = aSize / 2U - sizeof(MiniShmHeader);
    return NS_OK;
  }

  nsresult
  SetGuard(HANDLE aGuard, DWORD aTimeout)
  {
    if (!aGuard || !aTimeout) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    mGuard = aGuard;
    mTimeout = aTimeout;
    return NS_OK;
  }

  inline void
  SetObserver(MiniShmObserver *aObserver) { mObserver = aObserver; }

  /**
   * Obtains a writable pointer into shared memory of type T. This version 
   * differs from GetWritePtr in that it allows typename T to be one of 
   * the private data structures declared in MiniShmBase.
   *
   * @param aPtr Pointer to receive the shared memory address.
   *             This value is set if and only if the function 
   *             succeeded.
   * @return NS_OK if and only if aPtr was successfully obtained.
   *         NS_ERROR_ILLEGAL_VALUE if type T not an internal MiniShm struct.
   *         NS_ERROR_NOT_INITIALIZED if there is no valid MiniShm connection.
   */
  template<typename T> nsresult
  GetWritePtrInternal(T*& aPtr)
  {
    if (!mWriteHeader) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    if (sizeof(T) > mPayloadMaxLen ||
        T::identifier > RESERVED_CODE_LAST) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    mWriteHeader->mId = T::identifier;
    mWriteHeader->mPayloadLen = sizeof(T);
    aPtr = reinterpret_cast<T*>(mWriteHeader + 1);
    return NS_OK;
  }

  static VOID CALLBACK
  SOnEvent(PVOID aContext, BOOLEAN aIsTimer)
  {
    MiniShmBase* object = static_cast<MiniShmBase*>(aContext);
    object->OnEvent();
  }

private:
  MiniShmObserver*  mObserver;
  MiniShmHeader*    mWriteHeader;
  MiniShmHeader*    mReadHeader;
  unsigned int      mPayloadMaxLen;
  HANDLE            mGuard;
  DWORD             mTimeout;

  DISALLOW_COPY_AND_ASSIGN(MiniShmBase);
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_MiniShmBase_h

