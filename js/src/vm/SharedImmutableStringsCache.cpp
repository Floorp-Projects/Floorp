/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/SharedImmutableStringsCache-inl.h"

#include "jsstr.h"

namespace js {

SharedImmutableString::SharedImmutableString(SharedImmutableStringsCache&& cache,
                                             const char* chars,
                                             size_t length)
  : cache_(mozilla::Move(cache))
  , chars_(chars)
  , length_(length)
#ifdef DEBUG
  , hash_(mozilla::HashString(chars, length))
#endif
{
    MOZ_ASSERT(chars);
}

SharedImmutableString::SharedImmutableString(SharedImmutableString&& rhs)
  : cache_(mozilla::Move(rhs.cache_))
  , chars_(rhs.chars_)
  , length_(rhs.length_)
#ifdef DEBUG
  , hash_(mozilla::HashString(rhs.chars_, rhs.length_))
#endif
{
    MOZ_ASSERT(this != &rhs, "self move not allowed");
    MOZ_ASSERT(rhs.chars_);
    MOZ_ASSERT(rhs.hash_ == hash_);

    rhs.chars_ = nullptr;
}

SharedImmutableString&
SharedImmutableString::operator=(SharedImmutableString&& rhs) {
    this->~SharedImmutableString();
    new (this) SharedImmutableString(mozilla::Move(rhs));
    return *this;
}

SharedImmutableTwoByteString::SharedImmutableTwoByteString(SharedImmutableString&& string)
  : string_(mozilla::Move(string))
{ }

SharedImmutableTwoByteString::SharedImmutableTwoByteString(SharedImmutableStringsCache&& cache,
                                                           const char* chars,
                                                           size_t length)
  : string_(mozilla::Move(cache), chars, length)
{
    MOZ_ASSERT(length % sizeof(char16_t) == 0);
}

SharedImmutableTwoByteString::SharedImmutableTwoByteString(SharedImmutableTwoByteString&& rhs)
  : string_(mozilla::Move(rhs.string_))
{
    MOZ_ASSERT(this != &rhs, "self move not allowed");
}

SharedImmutableTwoByteString&
SharedImmutableTwoByteString::operator=(SharedImmutableTwoByteString&& rhs)
{
    this->~SharedImmutableTwoByteString();
    new (this) SharedImmutableTwoByteString(mozilla::Move(rhs));
    return *this;
}

SharedImmutableString::~SharedImmutableString() {
    if (!chars_)
        return;

    MOZ_ASSERT(mozilla::HashString(chars(), length()) == hash_);
    SharedImmutableStringsCache::Hasher::Lookup lookup(chars(), length());

    auto locked = cache_.inner_->lock();
    auto entry = locked->set.lookup(lookup);

    MOZ_ASSERT(entry);
    MOZ_ASSERT(entry->refcount > 0);

    entry->refcount--;
    if (entry->refcount == 0)
        locked->set.remove(entry);
}

SharedImmutableString
SharedImmutableString::clone() const
{
    auto clone = cache_.getOrCreate(chars(), length(), [&]() {
        MOZ_CRASH("Should not need to create an owned version, as this string is already in "
                  "the cache");
        return nullptr;
    });
    MOZ_ASSERT(clone.isSome());
    return SharedImmutableString(mozilla::Move(*clone));
}

SharedImmutableTwoByteString
SharedImmutableTwoByteString::clone() const
{
    return SharedImmutableTwoByteString(string_.clone());
}

MOZ_MUST_USE mozilla::Maybe<SharedImmutableString>
SharedImmutableStringsCache::getOrCreate(OwnedChars&& chars, size_t length)
{
    OwnedChars owned(mozilla::Move(chars));
    MOZ_ASSERT(owned);
    return getOrCreate(owned.get(), length, [&]() { return mozilla::Move(owned); });
}

MOZ_MUST_USE mozilla::Maybe<SharedImmutableString>
SharedImmutableStringsCache::getOrCreate(const char* chars, size_t length)
{
    return getOrCreate(chars, length, [&]() { return DuplicateString(chars, length); });
}

MOZ_MUST_USE mozilla::Maybe<SharedImmutableTwoByteString>
SharedImmutableStringsCache::getOrCreate(OwnedTwoByteChars&& chars, size_t length)
{
    OwnedTwoByteChars owned(mozilla::Move(chars));
    MOZ_ASSERT(owned);
    return getOrCreate(owned.get(), length, [&]() { return mozilla::Move(owned); });
}

MOZ_MUST_USE mozilla::Maybe<SharedImmutableTwoByteString>
SharedImmutableStringsCache::getOrCreate(const char16_t* chars, size_t length)
{
    return getOrCreate(chars, length, [&]() { return DuplicateString(chars, length); });
}

} // namespace js
