/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinSourceSupport_h
#define frontend_BinSourceSupport_h

#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"

#include "jsapi.h"

#include "frontend/BinToken.h"

#include "js/HashTable.h"
#include "js/Result.h"

namespace js {

// Support for parsing JS Binary ASTs.
struct BinaryASTSupport {
    using BinVariant  = js::frontend::BinVariant;
    using BinField = js::frontend::BinField;
    using BinKind  = js::frontend::BinKind;

    // A structure designed to perform fast char* + length lookup
    // without copies.
    struct CharSlice {
        const char* start_;
        uint32_t byteLen_;
        CharSlice(const CharSlice& other)
            : start_(other.start_)
            , byteLen_(other.byteLen_)
        {  }
        CharSlice(const char* start, const uint32_t byteLen)
            : start_(start)
            , byteLen_(byteLen)
        { }
        const char* begin() const {
            return start_;
        }
        const char* end() const {
            return start_ + byteLen_;
        }
#ifdef DEBUG
        void dump() const {
            for (auto c: *this)
                fprintf(stderr, "%c", c);

            fprintf(stderr, " (%d)", byteLen_);
        }
#endif // DEBUG

        typedef const CharSlice Lookup;
        static js::HashNumber hash(Lookup l) {
            return mozilla::HashString(l.start_, l.byteLen_);
        }
        static bool match(const Lookup key, Lookup lookup) {
            if (key.byteLen_ != lookup.byteLen_)
                return false;
            return strncmp(key.start_, lookup.start_, key.byteLen_) == 0;
        }
    };

    JS::Result<const BinVariant*>  binVariant(JSContext*, const CharSlice);
    JS::Result<const BinField*> binField(JSContext*, const CharSlice);
    JS::Result<const BinKind*> binKind(JSContext*,  const CharSlice);

  private:
    // A HashMap that can be queried without copies from a CharSlice key.
    // Initialized on first call. Keys are CharSlices into static strings.
    using BinKindMap = js::HashMap<const CharSlice, BinKind, CharSlice, js::SystemAllocPolicy>;
    BinKindMap binKindMap_;

    using BinFieldMap = js::HashMap<const CharSlice, BinField, CharSlice, js::SystemAllocPolicy>;
    BinFieldMap binFieldMap_;

    using BinVariantMap = js::HashMap<const CharSlice, BinVariant, CharSlice, js::SystemAllocPolicy>;
    BinVariantMap binVariantMap_;

};

} // namespace js

#endif // frontend_BinSourceSupport_h
