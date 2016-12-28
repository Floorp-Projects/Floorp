/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLValidateStrings.h"

#include "nsString.h"
#include "WebGLContext.h"

namespace mozilla {
// The following code was taken from the WebKit WebGL implementation,
// which can be found here:
// http://trac.webkit.org/browser/trunk/Source/WebCore/html/canvas/WebGLRenderingContext.cpp?rev=93625#L121
// Note that some modifications were done to adapt it to Mozilla.
/****** BEGIN CODE TAKEN FROM WEBKIT ******/
bool IsValidGLSLCharacter(char16_t c)
{
    // Printing characters are valid except " $ ` @ \ ' DEL.
    if (c >= 32 && c <= 126 &&
        c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
    {
         return true;
    }

    // Horizontal tab, line feed, vertical tab, form feed, carriage return
    // are also valid.
    if (c >= 9 && c <= 13) {
         return true;
    }

    return false;
}
/****** END CODE TAKEN FROM WEBKIT ******/

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
                                           nsString() };
    const nsString lineCommentEndings[] = { NS_LITERAL_STRING("\\\n"),
                                            NS_LITERAL_STRING("\n"),
                                            nsString() };
    const nsString blockCommentEndings[] = { NS_LITERAL_STRING("\n"),
                                             NS_LITERAL_STRING("*/"),
                                             nsString() };

    while (srcItr != srcEnd) {
        size_t foundId;
        fnEmitUntil( fnFindSoonestOf(commentBeginnings, 2, &foundId) );
        fnEmitUntil(srcItr + commentBeginnings[foundId].Length());

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

bool
ValidateGLSLString(const nsAString& string, WebGLContext* webgl, const char* funcName)
{
    for (size_t i = 0; i < string.Length(); ++i) {
        if (!IsValidGLSLCharacter(string.CharAt(i))) {
           webgl->ErrorInvalidValue("%s: String contains the illegal character '%d'",
                                    funcName, string.CharAt(i));
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
        webgl->ErrorInvalidValue("%s: Identifier is %d characters long, exceeds the"
                                 " maximum allowed length of %d characters.",
                                 funcName, name.Length(), maxSize);
        return false;
    }

    if (!ValidateGLSLString(name, webgl, funcName))
        return false;

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
