/* -*-  Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptPreloader_inl_h
#define ScriptPreloader_inl_h

#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Range.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsString.h"
#include "nsTArray.h"

#include <prio.h>

namespace mozilla {

namespace loader {

using mozilla::dom::AutoJSAPI;

static inline Result<Ok, nsresult>
Write(PRFileDesc* fd, const void* data, int32_t len)
{
    if (PR_Write(fd, data, len) != len) {
        return Err(NS_ERROR_FAILURE);
    }
    return Ok();
}

struct MOZ_RAII AutoSafeJSAPI : public AutoJSAPI
{
    AutoSafeJSAPI() { Init(); }
};


class OutputBuffer
{
public:
    OutputBuffer()
    {}

    uint8_t*
    write(size_t size)
    {
        auto buf = data.AppendElements(size);
        cursor_ += size;
        return buf;
    }

    void
    codeUint8(const uint8_t& val)
    {
        *write(sizeof val) = val;
    }

    template<typename T>
    void
    codeUint8(const EnumSet<T>& val)
    {
        // EnumSets are always represented as uint32_t values, so we need to
        // assert that the value actually fits in a uint8 before writing it.
        uint32_t value = val.serialize();
        codeUint8(CheckedUint8(value).value());
    }

    void
    codeUint16(const uint16_t& val)
    {
        LittleEndian::writeUint16(write(sizeof val), val);
    }

    void
    codeUint32(const uint32_t& val)
    {
        LittleEndian::writeUint32(write(sizeof val), val);
    }

    void
    codeString(const nsCString& str)
    {
        auto len = CheckedUint16(str.Length()).value();

        codeUint16(len);
        memcpy(write(len), str.BeginReading(), len);
    }

    size_t cursor() const { return cursor_; }


    uint8_t* Get() { return data.Elements(); }

    const uint8_t* Get() const { return data.Elements(); }

private:
    nsTArray<uint8_t> data;
    size_t cursor_ = 0;
};

class InputBuffer
{
public:
    explicit InputBuffer(const Range<uint8_t>& buffer)
        : data(buffer)
    {}

    const uint8_t*
    read(size_t size)
    {
        MOZ_ASSERT(checkCapacity(size));

        auto buf = &data[cursor_];
        cursor_ += size;
        return buf;
    }

    bool
    codeUint8(uint8_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = *read(sizeof val);
        }
        return !error_;
    }

    template<typename T>
    bool
    codeUint8(EnumSet<T>& val)
    {
        uint8_t value;
        if (codeUint8(value)) {
            val.deserialize(value);
        }
        return !error_;
    }

    bool
    codeUint16(uint16_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = LittleEndian::readUint16(read(sizeof val));
        }
        return !error_;
    }

    bool
    codeUint32(uint32_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = LittleEndian::readUint32(read(sizeof val));
        }
        return !error_;
    }

    bool
    codeString(nsCString& str)
    {
        uint16_t len;
        if (codeUint16(len)) {
            if (checkCapacity(len)) {
                str.SetLength(len);
                memcpy(str.BeginWriting(), read(len), len);
            }
        }
        return !error_;
    }

    bool error() { return error_; }

    bool finished() { return error_ || !remainingCapacity(); }

    size_t remainingCapacity() { return data.length() - cursor_; }

    size_t cursor() const { return cursor_; }

    const uint8_t* Get() const { return data.begin().get(); }

private:
    bool
    checkCapacity(size_t size)
    {
        if (size > remainingCapacity()) {
            error_ = true;
        }
        return !error_;
    }

    bool error_ = false;

public:
    const Range<uint8_t>& data;
    size_t cursor_ = 0;
};


template <typename T> struct Matcher;

// Wraps the iterator for a nsTHashTable so that it may be used as a range
// iterator. Each iterator result acts as a smart pointer to the hash element,
// and has a Remove() method which will remove the element from the hash.
//
// It also accepts an optional Matcher instance against which to filter the
// elements which should be iterated over.
//
// Example:
//
//    for (auto& elem : HashElemIter<HashType>(hash)) {
//        if (elem->IsDead()) {
//            elem.Remove();
//        }
//    }
template <typename T>
class HashElemIter
{
    using Iterator = typename T::Iterator;
    using ElemType = typename T::UserDataType;

    T& hash_;
    Matcher<ElemType>* matcher_;
    Maybe<Iterator> iter_;

public:
    explicit HashElemIter(T& hash, Matcher<ElemType>* matcher = nullptr)
        : hash_(hash), matcher_(matcher)
    {
        iter_.emplace(std::move(hash.Iter()));
    }

    class Elem
    {
        friend class HashElemIter<T>;

        HashElemIter<T>& iter_;
        bool done_;

        Elem(HashElemIter& iter, bool done)
            : iter_(iter), done_(done)
        {
            skipNonMatching();
        }

        Iterator& iter() { return iter_.iter_.ref(); }

        void skipNonMatching()
        {
            if (iter_.matcher_) {
                while (!done_ && !iter_.matcher_->Matches(get())) {
                    iter().Next();
                    done_ = iter().Done();
                }
            }
        }

    public:
        Elem& operator*() { return *this; }

        ElemType get()
        {
          if (done_) {
            return nullptr;
          }
          return iter().Data();
        }

        const ElemType get() const
        {
          return const_cast<Elem*>(this)->get();
        }

        ElemType operator->() { return get(); }

        const ElemType operator->() const { return get(); }

        operator ElemType() { return get(); }

        void Remove() { iter().Remove(); }

        Elem& operator++()
        {
            MOZ_ASSERT(!done_);

            iter().Next();
            done_ = iter().Done();

            skipNonMatching();
            return *this;
        }

        bool operator!=(Elem& other)
        {
            return done_ != other.done_ || this->get() != other.get();
        }
    };

    Elem begin() { return Elem(*this, iter_->Done()); }

    Elem end() { return Elem(*this, true); }
};

template <typename T>
HashElemIter<T> IterHash(T& hash, Matcher<typename T::UserDataType>* matcher = nullptr)
{
    return HashElemIter<T>(hash, matcher);
}

template <typename T, typename F>
bool
Find(T&& iter, F&& match)
{
    for (auto& elem : iter) {
        if (match(elem)) {
            return true;
        }
    }
    return false;
}

}; // namespace loader
}; // namespace mozilla

#endif // ScriptPreloader_inl_h
