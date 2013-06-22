/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef yarr_MatchResult_h
#define yarr_MatchResult_h

#include "wtfbridge.h"

typedef uint64_t EncodedMatchResult;

struct MatchResult {
    MatchResult(int start, int end)
        : start(start)
        , end(end)
    {
    }

#if !WTF_CPU_X86_64 || WTF_PLATFORM_WIN
    explicit MatchResult(EncodedMatchResult encoded)
    {
        union u {
            uint64_t encoded;
            struct s {
                int start;
                int end;
            } split;
        } value;
        value.encoded = encoded;
        start = value.split.start;
        end = value.split.end;
    }
#endif

    static MatchResult failed()
    {
        return MatchResult(int(WTF::notFound), 0);
    }

    operator bool()
    {
        return start != int(WTF::notFound);
    }

    bool empty()
    {
        return start == end;
    }

    // strings are limited to a length of 2^28. So this is safe
    int start;
    int end;
};

#endif /* yarr_MatchResult_h */
