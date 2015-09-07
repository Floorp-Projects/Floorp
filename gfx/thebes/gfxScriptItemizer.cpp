/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is based on usc_impl.c from ICU 4.2.0.1, slightly adapted
 * for use within Mozilla Gecko, separate from a standard ICU build.
 *
 * The original ICU license of the code follows:
 *
 * ICU License - ICU 1.8.1 and later
 *
 * COPYRIGHT AND PERMISSION NOTICE
 * 
 * Copyright (c) 1995-2009 International Business Machines Corporation and
 * others
 *
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software and that both the
 * above copyright notice(s) and this permission notice appear in supporting
 * documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
 * BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 *
 * All trademarks and registered trademarks mentioned herein are the property
 * of their respective owners. 
 */

#include "gfxScriptItemizer.h"
#include "nsUnicodeProperties.h"
#include "nsCharTraits.h"
#include "harfbuzz/hb.h"

#define MOD(sp) ((sp) % PAREN_STACK_DEPTH)
#define LIMIT_INC(sp) (((sp) < PAREN_STACK_DEPTH)? (sp) + 1 : PAREN_STACK_DEPTH)
#define INC(sp,count) (MOD((sp) + (count)))
#define INC1(sp) (INC(sp, 1))
#define DEC(sp,count) (MOD((sp) + PAREN_STACK_DEPTH - (count)))
#define DEC1(sp) (DEC(sp, 1))
#define STACK_IS_EMPTY() (pushCount <= 0)
#define STACK_IS_NOT_EMPTY() (! STACK_IS_EMPTY())
#define TOP() (parenStack[parenSP])
#define SYNC_FIXUP() (fixupCount = 0)

void
gfxScriptItemizer::push(uint32_t endPairChar, int32_t scriptCode)
{
    pushCount  = LIMIT_INC(pushCount);
    fixupCount = LIMIT_INC(fixupCount);

    parenSP = INC1(parenSP);
    parenStack[parenSP].endPairChar = endPairChar;
    parenStack[parenSP].scriptCode = scriptCode;
}

void
gfxScriptItemizer::pop()
{
    if (STACK_IS_EMPTY()) {
        return;
    }

    if (fixupCount > 0) {
        fixupCount -= 1;
    }

    pushCount -= 1;
    parenSP = DEC1(parenSP);
  
    /* If the stack is now empty, reset the stack
       pointers to their initial values.
     */
    if (STACK_IS_EMPTY()) {
        parenSP = -1;
    }
}

void
gfxScriptItemizer::fixup(int32_t scriptCode)
{
    int32_t fixupSP = DEC(parenSP, fixupCount);

    while (fixupCount-- > 0) {
        fixupSP = INC1(fixupSP);
        parenStack[fixupSP].scriptCode = scriptCode;
    }
}

static inline bool
SameScript(int32_t runScript, int32_t currCharScript)
{
    return runScript <= MOZ_SCRIPT_INHERITED ||
           currCharScript <= MOZ_SCRIPT_INHERITED ||
           currCharScript == runScript;
}

gfxScriptItemizer::gfxScriptItemizer(const char16_t *src, uint32_t length)
    : textPtr(src), textLength(length)
{
    reset();
}

void
gfxScriptItemizer::SetText(const char16_t *src, uint32_t length)
{
    textPtr  = src;
    textLength = length;

    reset();
}

bool
gfxScriptItemizer::Next(uint32_t& aRunStart, uint32_t& aRunLimit,
                        int32_t& aRunScript)
{
    /* if we've fallen off the end of the text, we're done */
    if (scriptLimit >= textLength) {
        return false;
    }

    SYNC_FIXUP();
    scriptCode = MOZ_SCRIPT_COMMON;

    for (scriptStart = scriptLimit; scriptLimit < textLength; scriptLimit += 1) {
        uint32_t ch;
        int32_t sc;
        uint32_t startOfChar = scriptLimit;

        ch = textPtr[scriptLimit];

        /* decode UTF-16 (may be surrogate pair) */
        if (NS_IS_HIGH_SURROGATE(ch) && scriptLimit < textLength - 1) {
            uint32_t low = textPtr[scriptLimit + 1];
            if (NS_IS_LOW_SURROGATE(low)) {
                ch = SURROGATE_TO_UCS4(ch, low);
                scriptLimit += 1;
            }
        }

        // Get the nsCharProps2 record for the current character,
        // so we can read the script and (if needed) the gen category
        // without needing to do two multi-level lookups.
        // NOTE that this means we're relying on an implementation detail
        // of the nsUnicodeProperties tables, and might have to revise this
        // if the nsCharProps records used there are modified in future.
        const nsCharProps2& charProps = GetCharProps2(ch);

        // Initialize gc to UNASSIGNED; we'll only set it to the true GC
        // if the character has script=COMMON, otherwise we don't care.
        uint8_t gc = HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED;

        sc = charProps.mScriptCode;
        if (sc == MOZ_SCRIPT_COMMON) {
            /*
             * Paired character handling:
             *
             * if it's an open character, push it onto the stack.
             * if it's a close character, find the matching open on the
             * stack, and use that script code. Any non-matching open
             * characters above it on the stack will be popped.
             *
             * We only do this if the script is COMMON; for chars with
             * specific script assignments, we just use them as-is.
             */
            gc = charProps.mCategory;
            if (gc == HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION) {
                uint32_t endPairChar = mozilla::unicode::GetMirroredChar(ch);
                if (endPairChar != ch) {
                    push(endPairChar, scriptCode);
                }
            } else if (gc == HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION &&
                HasMirroredChar(ch))
            {
                while (STACK_IS_NOT_EMPTY() && TOP().endPairChar != ch) {
                    pop();
                }

                if (STACK_IS_NOT_EMPTY()) {
                    sc = TOP().scriptCode;
                }
            }
        }

        if (SameScript(scriptCode, sc)) {
            if (scriptCode <= MOZ_SCRIPT_INHERITED &&
                sc > MOZ_SCRIPT_INHERITED)
            {
                scriptCode = sc;
                fixup(scriptCode);
            }

            /*
             * if this character is a close paired character,
             * pop the matching open character from the stack
             */
            if (gc == HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION &&
                HasMirroredChar(ch)) {
                pop();
            }
        } else {
            /*
             * reset scriptLimit in case it was advanced during reading a
             * multiple-code-unit character
             */
            scriptLimit = startOfChar;

            break;
        }
    }

    aRunStart = scriptStart;
    aRunLimit = scriptLimit;
    aRunScript = scriptCode;

    return true;
}
