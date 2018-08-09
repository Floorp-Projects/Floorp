/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLValidateStrings.h"

#include "WebGLContext.h"

namespace mozilla {

bool
TruncateComments(const nsAString& src, nsAString* const out)
{
    const size_t dstByteCount = src.Length() * sizeof(src[0]);
    const UniqueBuffer dst(malloc(dstByteCount));
    if (!dst)
        return false;

    auto srcItr = src.BeginReading();
    const auto srcEnd = src.EndReading();
    const auto dstBegin = (decltype(src[0])*)dst.get();
    auto dstItr = dstBegin;

    const auto fnEmitUntil = [&](const decltype(srcItr)& nextSrcItr) {
        while (srcItr != nextSrcItr) {
            *dstItr = *srcItr;
            ++srcItr;
            ++dstItr;
        }
    };

    const auto fnFindSoonestOf = [&](const nsString* needles, size_t needleCount,
                                     size_t* const out_foundId)
    {
        auto foundItr = srcItr;
        while (foundItr != srcEnd) {
            const auto haystack = Substring(foundItr, srcEnd);
            for (size_t i = 0; i < needleCount; i++) {
                if (StringBeginsWith(haystack, needles[i])) {
                    *out_foundId = i;
                    return foundItr;
                }
            }
            ++foundItr;
        }
        *out_foundId = needleCount;
        return foundItr;
    };

    ////

    const nsString commentBeginnings[] = { NS_LITERAL_STRING("//"),
                                           NS_LITERAL_STRING("/*"),
                                           nsString() }; // Final empty string for "found
                                                         // nothing".
    const nsString lineCommentEndings[] = { NS_LITERAL_STRING("\\\n"),
                                            NS_LITERAL_STRING("\n"),
                                            nsString() };
    const nsString blockCommentEndings[] = { NS_LITERAL_STRING("\n"),
                                             NS_LITERAL_STRING("*/"),
                                             nsString() };

    while (srcItr != srcEnd) {
        size_t foundId;
        fnEmitUntil( fnFindSoonestOf(commentBeginnings, 2, &foundId) );
        fnEmitUntil(srcItr + commentBeginnings[foundId].Length()); // Final empty string
                                                                   // allows us to skip
                                                                   // forward here
                                                                   // unconditionally.
        switch (foundId) {
        case 0: // line comment
            while (true) {
                size_t endId;
                srcItr = fnFindSoonestOf(lineCommentEndings, 2, &endId);
                fnEmitUntil(srcItr + lineCommentEndings[endId].Length());
                if (endId == 0)
                    continue;
                break;
            }
            break;

        case 1: // block comment
            while (true) {
                size_t endId;
                srcItr = fnFindSoonestOf(blockCommentEndings, 2, &endId);
                fnEmitUntil(srcItr + blockCommentEndings[endId].Length());
                if (endId == 0)
                    continue;
                break;
            }
            break;

        default: // not found
            break;
        }
    }

    MOZ_ASSERT((dstBegin+1) - dstBegin == 1);
    const uint32_t dstCharLen = dstItr - dstBegin;
    if (!out->Assign(dstBegin, dstCharLen, mozilla::fallible))
        return false;

    return true;
}

////////////////////////////////////////////////////////////////////////////////

static bool
IsValidGLSLChar(char16_t c)
{
    if (('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        ('0' <= c && c <= '9'))
    {
        return true;
    }

    switch (c) {
    case ' ':
    case '\t':
    case '\v':
    case '\f':
    case '\r':
    case '\n':
    case '_':
    case '.':
    case '+':
    case '-':
    case '/':
    case '*':
    case '%':
    case '<':
    case '>':
    case '[':
    case ']':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '|':
    case '&':
    case '~':
    case '=':
    case '!':
    case ':':
    case ';':
    case ',':
    case '?':
        return true;

    default:
        return false;
    }
}

static bool
IsValidGLSLPreprocChar(char16_t c)
{
    if (IsValidGLSLChar(c))
        return true;

    switch (c) {
    case '\\':
    case '#':
        return true;

    default:
        return false;
    }
}

////

bool
ValidateGLSLPreprocString(WebGLContext* webgl, const char* funcName,
                          const nsAString& string)
{
    for (size_t i = 0; i < string.Length(); ++i) {
        const auto& cur = string[i];

        if (!IsValidGLSLPreprocChar(cur)) {
            webgl->ErrorInvalidValue("%s: String contains the illegal character 0x%x.",
                                     funcName, cur);
            return false;
        }

        if (cur == '\\' && !webgl->IsWebGL2()) {
            // Todo: Backslash is technically still invalid in WebGLSL 1 under even under
            // WebGL 2.
            webgl->ErrorInvalidValue("%s: Backslash is not valid in WebGL 1.", funcName);
            return false;
        }
    }

    return true;
}

bool
ValidateGLSLVariableName(const nsAString& name, WebGLContext* webgl, const char* funcName)
{
    if (name.IsEmpty())
        return false;

    const uint32_t maxSize = webgl->IsWebGL2() ? 1024 : 256;
    if (name.Length() > maxSize) {
        webgl->ErrorInvalidValue("%s: Identifier is %u characters long, exceeds the"
                                 " maximum allowed length of %u characters.",
                                 funcName, name.Length(), maxSize);
        return false;
    }

    for (size_t i = 0; i < name.Length(); ++i) {
        const auto& cur = name[i];
        if (!IsValidGLSLChar(cur)) {
           webgl->ErrorInvalidValue("%s: String contains the illegal character 0x%x'.",
                                    funcName, cur);
           return false;
        }
    }

    nsString prefix1 = NS_LITERAL_STRING("webgl_");
    nsString prefix2 = NS_LITERAL_STRING("_webgl_");

    if (Substring(name, 0, prefix1.Length()).Equals(prefix1) ||
        Substring(name, 0, prefix2.Length()).Equals(prefix2))
    {
        webgl->ErrorInvalidOperation("%s: String contains a reserved GLSL prefix.",
                                     funcName);
        return false;
    }

    return true;
}

} // namespace mozilla
