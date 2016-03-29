/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SharedImmutableStringsCache_h
#define vm_SharedImmutableStringsCache_h

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

#include <cstring>
#include <new> // for placement new

#include "jsstr.h"

#include "js/HashTable.h"
#include "js/Utility.h"

#include "threading/ExclusiveData.h"

namespace js {

class SharedImmutableStringsCache;
class SharedImmutableTwoByteString;

/**
 * The `SharedImmutableString` class holds a reference to a `const char*` string
 * from the `SharedImmutableStringsCache` and releases the reference upon
 * destruction.
 */
class SharedImmutableString
{
    friend class SharedImmutableStringsCache;
    friend class SharedImmutableTwoByteString;

    // Never nullptr in a live instance. May be nullptr if this instance has
    // been moved from.
    SharedImmutableStringsCache* cache_;
    const char* chars_;
    size_t length_;
#ifdef DEBUG
    HashNumber hash_;
#endif

    SharedImmutableString(SharedImmutableStringsCache* cache, const char* chars, size_t length)
      : cache_(cache)
      , chars_(chars)
      , length_(length)
#ifdef DEBUG
      , hash_(mozilla::HashString(chars, length))
#endif
    {
        MOZ_ASSERT(cache && chars);
    }

  public:
    /**
     * `SharedImmutableString`s are move-able. It is an error to use a
     * `SharedImmutableString` after it has been moved.
     */
    SharedImmutableString(SharedImmutableString&& rhs)
      : cache_(rhs.cache_)
      , chars_(rhs.chars_)
      , length_(rhs.length_)
#ifdef DEBUG
      , hash_(mozilla::HashString(rhs.chars_, rhs.length_))
#endif
    {
        MOZ_ASSERT(this != &rhs, "self move not allowed");
        MOZ_ASSERT(rhs.cache_ && rhs.chars_);
        MOZ_ASSERT(rhs.hash_ == hash_);

        rhs.cache_ = nullptr;
        rhs.chars_ = nullptr;
    }
    SharedImmutableString& operator=(SharedImmutableString&& rhs) {
        this->~SharedImmutableString();
        new (this) SharedImmutableString(mozilla::Move(rhs));
        return *this;
    }

    /**
     * Create another shared reference to the underlying string.
     */
    SharedImmutableString clone() const;

    ~SharedImmutableString();

    /**
     * Get a raw pointer to the underlying string. It is only safe to use the
     * resulting pointer while this `SharedImmutableString` exists.
     */
    const char* chars() const {
        MOZ_ASSERT(cache_ && chars_);
        return chars_;
    }

    /**
     * Get the length of the underlying string.
     */
    size_t length() const {
        MOZ_ASSERT(cache_ && chars_);
        return length_;
    }
};

/**
 * The `SharedImmutableTwoByteString` class holds a reference to a `const
 * char16_t*` string from the `SharedImmutableStringsCache` and releases the
 * reference upon destruction.
 */
class SharedImmutableTwoByteString
{
    friend class SharedImmutableStringsCache;

    // If a `char*` string and `char16_t*` string happen to have the same bytes,
    // the bytes will be shared but handed out as different types.
    SharedImmutableString string_;

    explicit SharedImmutableTwoByteString(SharedImmutableString&& string)
      : string_(mozilla::Move(string))
    { }

    SharedImmutableTwoByteString(SharedImmutableStringsCache* cache, const char* chars, size_t length)
      : string_(cache, chars, length)
    {
        MOZ_ASSERT(length % sizeof(char16_t) == 0);
    }

  public:
    /**
     * `SharedImmutableTwoByteString`s are move-able. It is an error to use a
     * `SharedImmutableTwoByteString` after it has been moved.
     */
    SharedImmutableTwoByteString(SharedImmutableTwoByteString&& rhs)
      : string_(mozilla::Move(rhs.string_))
    {
        MOZ_ASSERT(this != &rhs, "self move not allowed");
    }
    SharedImmutableTwoByteString& operator=(SharedImmutableTwoByteString&& rhs) {
        this->~SharedImmutableTwoByteString();
        new (this) SharedImmutableTwoByteString(mozilla::Move(rhs));
        return *this;
    }

    /**
     * Create another shared reference to the underlying string.
     */
    SharedImmutableTwoByteString clone() const;

    /**
     * Get a raw pointer to the underlying string. It is only safe to use the
     * resulting pointer while this `SharedImmutableTwoByteString` exists.
     */
    const char16_t* chars() const { return reinterpret_cast<const char16_t*>(string_.chars()); }

    /**
     * Get the length of the underlying string.
     */
    size_t length() const { return string_.length() / sizeof(char16_t); }
};

/**
 * The `SharedImmutableStringsCache` allows for safely sharing and deduplicating
 * immutable strings (either `const char*` or `const char16_t*`) between
 * threads.
 *
 * The locking mechanism is dead-simple and coarse grained: a single lock guards
 * all of the internal table itself, the table's entries, and the entries'
 * reference counts. It is only safe to perform any mutation on the cache or any
 * data stored within the cache when this lock is acquired.
 */
class SharedImmutableStringsCache
{
    friend class SharedImmutableString;
    struct Hasher;

  public:
    using OwnedChars = mozilla::UniquePtr<char[], JS::FreePolicy>;
    using OwnedTwoByteChars = mozilla::UniquePtr<char16_t[], JS::FreePolicy>;

    /**
     * Get the canonical, shared, and de-duplicated version of the given `const
     * char*` string. If such a string does not exist, call `intoOwnedChars` and
     * add the string it returns to the cache.
     *
     * `intoOwnedChars` must create an owned version of the given string, and
     * must have one of the following types:
     *
     *     mozilla::UniquePtr<char[], JS::FreePolicy>   intoOwnedChars();
     *     mozilla::UniquePtr<char[], JS::FreePolicy>&& intoOwnedChars();
     *
     * It can be used by callers to elide a copy of the string when it is safe
     * to give up ownership of the lookup string to the cache. It must return a
     * `nullptr` on failure.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    template <typename IntoOwnedChars>
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableString>
    getOrCreate(const char* chars, size_t length, IntoOwnedChars intoOwnedChars) {
        Hasher::Lookup lookup(chars, length);

        auto locked = set_.lock();
        if (!locked->initialized() && !locked->init())
            return mozilla::Nothing();

        auto entry = locked->lookupForAdd(lookup);
        if (!entry) {
            OwnedChars ownedChars(intoOwnedChars());
            if (!ownedChars)
                return mozilla::Nothing();
            MOZ_ASSERT(ownedChars.get() == chars ||
                       memcmp(ownedChars.get(), chars, length) == 0);
            StringBox box(mozilla::Move(ownedChars), length);
            if (!locked->add(entry, mozilla::Move(box)))
                return mozilla::Nothing();
        }

        MOZ_ASSERT(entry);
        entry->refcount++;
        return mozilla::Some(SharedImmutableString(this, entry->chars(),
                                                   entry->length()));
    }

    /**
     * Take ownership of the given `chars` and return the canonical, shared and
     * de-duplicated version.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableString>
    getOrCreate(OwnedChars&& chars, size_t length);

    /**
     * Do not take ownership of the given `chars`. Return the canonical, shared
     * and de-duplicated version. If there is no extant shared version of
     * `chars`, make a copy and insert it into the cache.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableString>
    getOrCreate(const char* chars, size_t length);

    /**
     * Get the canonical, shared, and de-duplicated version of the given `const
     * char16_t*` string. If such a string does not exist, call `intoOwnedChars`
     * and add the string it returns to the cache.
     *
     * `intoOwnedTwoByteChars` must create an owned version of the given string,
     * and must have one of the following types:
     *
     *     mozilla::UniquePtr<char16_t[], JS::FreePolicy>   intoOwnedTwoByteChars();
     *     mozilla::UniquePtr<char16_t[], JS::FreePolicy>&& intoOwnedTwoByteChars();
     *
     * It can be used by callers to elide a copy of the string when it is safe
     * to give up ownership of the lookup string to the cache. It must return a
     * `nullptr` on failure.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    template <typename IntoOwnedTwoByteChars>
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableTwoByteString>
    getOrCreate(const char16_t* chars, size_t length, IntoOwnedTwoByteChars intoOwnedTwoByteChars) {
        Hasher::Lookup lookup(chars, length);

        auto locked = set_.lock();
        if (!locked->initialized() && !locked->init())
            return mozilla::Nothing();

        auto entry = locked->lookupForAdd(lookup);
        if (!entry) {
            OwnedTwoByteChars ownedTwoByteChars(intoOwnedTwoByteChars());
            if (!ownedTwoByteChars)
                return mozilla::Nothing();
            MOZ_ASSERT(ownedTwoByteChars.get() == chars ||
                       memcmp(ownedTwoByteChars.get(), chars, length * sizeof(char16_t)) == 0);
            OwnedChars ownedChars(reinterpret_cast<char*>(ownedTwoByteChars.release()));
            StringBox box(mozilla::Move(ownedChars), length * sizeof(char16_t));
            if (!locked->add(entry, mozilla::Move(box)))
                return mozilla::Nothing();
        }

        MOZ_ASSERT(entry);
        entry->refcount++;
        return mozilla::Some(SharedImmutableTwoByteString(this, entry->chars(),
                                                          entry->length()));
    }

    /**
     * Take ownership of the given `chars` and return the canonical, shared and
     * de-duplicated version.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableTwoByteString>
    getOrCreate(OwnedTwoByteChars&& chars, size_t length);

    /**
     * Do not take ownership of the given `chars`. Return the canonical, shared
     * and de-duplicated version. If there is no extant shared version of
     * `chars`, then make a copy and insert it into the cache.
     *
     * On success, `Some` is returned. In the case of OOM failure, `Nothing` is
     * returned.
     */
    MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableTwoByteString>
    getOrCreate(const char16_t* chars, size_t length);

    SharedImmutableStringsCache()
      : set_(Set())
    { }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        size_t n = 0;

        auto locked = set_.lock();
        if (!locked->initialized())
            return n;

        // Size of the table.
        n += locked->sizeOfExcludingThis(mallocSizeOf);

        // Sizes of the strings.
        for (auto r = locked->all(); !r.empty(); r.popFront())
            n += mallocSizeOf(r.front().chars());

        return n;
    }

  private:
    class StringBox
    {
        OwnedChars chars_;
        size_t length_;

      public:
        mutable size_t refcount;

        StringBox(OwnedChars&& chars, size_t length)
          : chars_(mozilla::Move(chars))
          , length_(length)
          , refcount(0)
        {
            MOZ_ASSERT(chars_);
        }

        StringBox(StringBox&& rhs)
          : chars_(mozilla::Move(rhs.chars_))
          , length_(rhs.length_)
          , refcount(rhs.refcount)
        {
            MOZ_ASSERT(this != &rhs, "self move not allowed");
            rhs.refcount = 0;
        }

        ~StringBox() {
            MOZ_ASSERT(refcount == 0);
        }

        const char* chars() const { return chars_.get(); }
        size_t length() const { return length_; }
    };

    struct Hasher
    {
        /**
         * A structure used when querying for a `const char*` string in the cache.
         */
        class Lookup
        {
            friend struct Hasher;

            const char* chars_;
            size_t length_;

          public:
            Lookup(const char* chars, size_t length)
              : chars_(chars)
              , length_(length)
            {
                MOZ_ASSERT(chars_);
            }

            explicit Lookup(const char* chars)
              : Lookup(chars, strlen(chars))
            { }

            Lookup(const char16_t* chars, size_t length)
              : Lookup(reinterpret_cast<const char*>(chars), length * sizeof(char16_t))
            { }

            explicit Lookup(const char16_t* chars)
              : Lookup(chars, js_strlen(chars))
            { }
        };

        static HashNumber hash(const Lookup& lookup) {
            MOZ_ASSERT(lookup.chars_);
            return mozilla::HashString(lookup.chars_, lookup.length_);
        }

        static bool match(const StringBox& key, const Lookup& lookup) {
            MOZ_ASSERT(lookup.chars_);
            MOZ_ASSERT(key.chars());

            if (key.length() != lookup.length_)
                return false;

            if (key.chars() == lookup.chars_)
                return true;

            return memcmp(key.chars(), lookup.chars_, key.length()) == 0;
        }
    };

    using Set = HashSet<StringBox, Hasher, SystemAllocPolicy>;
    ExclusiveData<Set> set_;
};

} // namespace js

#endif // vm_SharedImmutableStringsCache_h
