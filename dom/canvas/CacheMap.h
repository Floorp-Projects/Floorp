/* -*- Mode: C++; tab-width: 13; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=13 sts=4 et sw=4 tw=90: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CACHE_MAP_H_
#define MOZILLA_CACHE_MAP_H_

#include "mozilla/UniquePtr.h"
#include <map>
#include <unordered_set>
#include <vector>

namespace mozilla {

namespace detail {
class CacheMapUntypedEntry;
}

class CacheMapInvalidator
{
    friend class detail::CacheMapUntypedEntry;

    mutable std::unordered_set<const detail::CacheMapUntypedEntry*> mCacheEntries;

public:
    ~CacheMapInvalidator() {
        InvalidateCaches();
    }

    void InvalidateCaches() const;
};

namespace detail {

class CacheMapUntypedEntry
{
    template<typename, typename> friend class CacheMap;

private:
    const std::vector<const CacheMapInvalidator*> mInvalidators;

protected:
    CacheMapUntypedEntry(std::vector<const CacheMapInvalidator*>&& invalidators);
    ~CacheMapUntypedEntry();

public:
    virtual void Invalidate() const = 0;
};

struct DerefLess final {
    template<typename T>
    bool operator ()(const T* const a, const T* const b) const {
        return *a < *b;
    }
};

} // namespace detail


template<typename KeyT, typename ValueT>
class CacheMap final
{
    class Entry final : public detail::CacheMapUntypedEntry {
    public:
        CacheMap& mParent;
        const KeyT mKey;
        const ValueT mValue;

        Entry(std::vector<const CacheMapInvalidator*>&& invalidators, CacheMap& parent,
              KeyT&& key, ValueT&& value)
            : detail::CacheMapUntypedEntry(std::move(invalidators))
            , mParent(parent)
            , mKey(std::move(key))
            , mValue(std::move(value))
        { }

        void Invalidate() const override {
            const auto erased = mParent.mMap.erase(&mKey);
            MOZ_ALWAYS_TRUE( erased == 1 );
        }

        bool operator <(const Entry& x) const {
            return mKey < x.mKey;
        }
    };

    typedef std::map<const KeyT*, UniquePtr<const Entry>, detail::DerefLess> MapT;
    MapT mMap;

public:
    const ValueT* Insert(KeyT&& key, ValueT&& value,
                         std::vector<const CacheMapInvalidator*>&& invalidators)
    {
        UniquePtr<const Entry> entry( new Entry(std::move(invalidators), *this, std::move(key),
                                                std::move(value)) );

        typename MapT::value_type insertable{
            &entry->mKey,
            nullptr
        };
        insertable.second = std::move(entry);

        const auto res = mMap.insert(std::move(insertable));
        const auto& didInsert = res.second;
        MOZ_ALWAYS_TRUE( didInsert );

        const auto& itr = res.first;
        return &itr->second->mValue;
    }

    const ValueT* Find(const KeyT& key) const {
        const auto itr = mMap.find(&key);
        if (itr == mMap.end())
            return nullptr;

        return &itr->second->mValue;
    }

    void Invalidate() {
        while (mMap.size()) {
            const auto& itr = mMap.begin();
            itr->second->Invalidate();
        }
    }
};

} // namespace mozilla

#endif // MOZILLA_CACHE_MAP_H_
