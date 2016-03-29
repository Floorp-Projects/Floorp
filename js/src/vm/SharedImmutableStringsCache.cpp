/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/SharedImmutableStringsCache.h"

#include "jsstr.h"

namespace js {

SharedImmutableString::~SharedImmutableString() {
    MOZ_ASSERT(!!cache_ == !!chars_);
    if (!cache_)
        return;

    MOZ_ASSERT(mozilla::HashString(chars(), length()) == hash_);
    SharedImmutableStringsCache::Hasher::Lookup lookup(chars(), length());

    auto locked = cache_->set_.lock();
    auto entry = locked->lookup(lookup);

    MOZ_ASSERT(entry);
    MOZ_ASSERT(entry->refcount > 0);

    entry->refcount--;
    if (entry->refcount == 0)
        locked->remove(entry);
}

SharedImmutableString
SharedImmutableString::clone() const
{
    auto clone = cache_->getOrCreate(chars(), length(), [&]() {
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

MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableString>
SharedImmutableStringsCache::getOrCreate(OwnedChars&& chars, size_t length)
{
    OwnedChars owned(mozilla::Move(chars));
    return getOrCreate(owned.get(), length, [&]() { return mozilla::Move(owned); });
}

MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableString>
SharedImmutableStringsCache::getOrCreate(const char* chars, size_t length)
{
    return getOrCreate(chars, length, [&]() { return DuplicateString(chars, length); });
}

MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableTwoByteString>
SharedImmutableStringsCache::getOrCreate(OwnedTwoByteChars&& chars, size_t length)
{
    OwnedTwoByteChars owned(mozilla::Move(chars));
    return getOrCreate(owned.get(), length, [&]() { return mozilla::Move(owned); });
}

MOZ_WARN_UNUSED_RESULT mozilla::Maybe<SharedImmutableTwoByteString>
SharedImmutableStringsCache::getOrCreate(const char16_t* chars, size_t length)
{
    return getOrCreate(chars, length, [&]() { return DuplicateString(chars, length); });
}

} // namespace js
