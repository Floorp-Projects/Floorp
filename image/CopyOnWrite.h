/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * CopyOnWrite<T> allows code to safely read from a data structure without
 * worrying that reentrant code will modify it.
 */

#ifndef mozilla_image_CopyOnWrite_h
#define mozilla_image_CopyOnWrite_h

#include "mozilla/nsRefPtr.h"
#include "MainThreadUtils.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Implementation Details
///////////////////////////////////////////////////////////////////////////////

namespace detail {

template <typename T>
class CopyOnWriteValue final
{
public:
  NS_INLINE_DECL_REFCOUNTING(CopyOnWriteValue)

  explicit CopyOnWriteValue(T* aValue) : mValue(aValue) { }
  explicit CopyOnWriteValue(already_AddRefed<T>& aValue) : mValue(aValue) { }
  explicit CopyOnWriteValue(already_AddRefed<T>&& aValue) : mValue(aValue) { }
  explicit CopyOnWriteValue(const nsRefPtr<T>& aValue) : mValue(aValue) { }
  explicit CopyOnWriteValue(nsRefPtr<T>&& aValue) : mValue(aValue) { }

  T* get() { return mValue.get(); }
  const T* get() const { return mValue.get(); }

  bool HasReaders() const { return mReaders > 0; }
  bool HasWriter() const { return mWriter; }
  bool HasUsers() const { return HasReaders() || HasWriter(); }

  void LockForReading() { MOZ_ASSERT(!HasWriter()); mReaders++; }
  void UnlockForReading() { MOZ_ASSERT(HasReaders()); mReaders--; }

  struct MOZ_STACK_CLASS AutoReadLock
  {
    explicit AutoReadLock(CopyOnWriteValue* aValue)
      : mValue(aValue)
    {
      mValue->LockForReading();
    }
    ~AutoReadLock() { mValue->UnlockForReading(); }
    CopyOnWriteValue<T>* mValue;
  };

  void LockForWriting() { MOZ_ASSERT(!HasUsers()); mWriter = true; }
  void UnlockForWriting() { MOZ_ASSERT(HasWriter()); mWriter = false; }

  struct MOZ_STACK_CLASS AutoWriteLock
  {
    explicit AutoWriteLock(CopyOnWriteValue* aValue)
      : mValue(aValue)
    {
      mValue->LockForWriting();
    }
    ~AutoWriteLock() { mValue->UnlockForWriting(); }
    CopyOnWriteValue<T>* mValue;
  };

private:
  CopyOnWriteValue(const CopyOnWriteValue&) = delete;
  CopyOnWriteValue(CopyOnWriteValue&&) = delete;

  ~CopyOnWriteValue() { }

  nsRefPtr<T> mValue;
  uint64_t mReaders = 0;
  bool mWriter = false;
};

} // namespace detail


///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/**
 * CopyOnWrite<T> allows code to safely read from a data structure without
 * worrying that reentrant code will modify it. If reentrant code would modify
 * the data structure while other code is reading from it, a copy is made so
 * that readers can continue to use the old version.
 *
 * Note that it's legal to nest a writer inside any number of readers, but
 * nothing can be nested inside a writer. This is because it's assumed that the
 * state of the contained data structure may not be consistent during the write.
 *
 * This is a main-thread-only data structure.
 *
 * To work with CopyOnWrite<T>, a type T needs to be reference counted and to
 * support copy construction.
 */
template <typename T>
class CopyOnWrite final
{
  typedef detail::CopyOnWriteValue<T> CopyOnWriteValue;

public:
  explicit CopyOnWrite(T* aValue)
  : mValue(new CopyOnWriteValue(aValue))
  { }

  explicit CopyOnWrite(already_AddRefed<T>& aValue)
    : mValue(new CopyOnWriteValue(aValue))
  { }

  explicit CopyOnWrite(already_AddRefed<T>&& aValue)
    : mValue(new CopyOnWriteValue(aValue))
  { }

  explicit CopyOnWrite(const nsRefPtr<T>& aValue)
    : mValue(new CopyOnWriteValue(aValue))
  { }

  explicit CopyOnWrite(nsRefPtr<T>&& aValue)
    : mValue(new CopyOnWriteValue(aValue))
  { }

  /// @return true if it's safe to read at this time.
  bool CanRead() const { return !mValue->HasWriter(); }

  /**
   * Read from the contained data structure using the function @aReader.
   * @aReader will be passed a pointer of type |const T*|. It's not legal to
   * call this while a writer is active.
   *
   * @return whatever value @aReader returns, or nothing if @aReader is a void
   *         function.
   */
  template <typename ReadFunc>
  auto Read(ReadFunc aReader) const
    -> decltype(aReader(static_cast<const T*>(nullptr)))
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(CanRead());

    // Run the provided function while holding a read lock.
    nsRefPtr<CopyOnWriteValue> cowValue = mValue;
    typename CopyOnWriteValue::AutoReadLock lock(cowValue);
    return aReader(cowValue->get());
  }

  /**
   * Read from the contained data structure using the function @aReader.
   * @aReader will be passed a pointer of type |const T*|. If it's currently not
   * possible to read because a writer is currently active, @aOnError will be
   * called instead.
   *
   * @return whatever value @aReader or @aOnError returns (their return types
   *         must be consistent), or nothing if the provided functions are void.
   */
  template <typename ReadFunc, typename ErrorFunc>
  auto Read(ReadFunc aReader, ErrorFunc aOnError) const
    -> decltype(aReader(static_cast<const T*>(nullptr)))
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!CanRead()) {
      return aOnError();
    }

    return Read(aReader);
  }

  /// @return true if it's safe to write at this time.
  bool CanWrite() const { return !mValue->HasWriter(); }

  /**
   * Write to the contained data structure using the function @aWriter.
   * @aWriter will be passed a pointer of type |T*|. It's not legal to call this
   * while another writer is active.
   *
   * If readers are currently active, they will be able to continue reading from
   * a copy of the old version of the data structure. The copy will be destroyed
   * when all its readers finish.  Later readers and writers will see the
   * version of the data structure produced by the most recent call to Write().
   *
   * @return whatever value @aWriter returns, or nothing if @aWriter is a void
   *         function.
   */
  template <typename WriteFunc>
  auto Write(WriteFunc aWriter)
    -> decltype(aWriter(static_cast<T*>(nullptr)))
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(CanWrite());

    // If there are readers, we need to copy first.
    if (mValue->HasReaders()) {
      mValue = new CopyOnWriteValue(new T(*mValue->get()));
    }

    // Run the provided function while holding a write lock.
    nsRefPtr<CopyOnWriteValue> cowValue = mValue;
    typename CopyOnWriteValue::AutoWriteLock lock(cowValue);
    return aWriter(cowValue->get());
  }

  /**
   * Write to the contained data structure using the function @aWriter.
   * @aWriter will be passed a pointer of type |T*|. If it's currently not
   * possible to write because a writer is currently active, @aOnError will be
   * called instead.
   *
   * If readers are currently active, they will be able to continue reading from
   * a copy of the old version of the data structure. The copy will be destroyed
   * when all its readers finish.  Later readers and writers will see the
   * version of the data structure produced by the most recent call to Write().
   *
   * @return whatever value @aWriter or @aOnError returns (their return types
   *         must be consistent), or nothing if the provided functions are void.
   */
  template <typename WriteFunc, typename ErrorFunc>
  auto Write(WriteFunc aWriter, ErrorFunc aOnError)
    -> decltype(aWriter(static_cast<T*>(nullptr)))
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!CanWrite()) {
      return aOnError();
    }

    return Write(aWriter);
  }

private:
  CopyOnWrite(const CopyOnWrite&) = delete;
  CopyOnWrite(CopyOnWrite&&) = delete;

  nsRefPtr<CopyOnWriteValue> mValue;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_CopyOnWrite_h
