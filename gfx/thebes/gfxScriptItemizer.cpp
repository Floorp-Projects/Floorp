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
#include "gfxFontUtils.h" // for the FindHighestBit function
#include "nsUnicodeProperties.h"

#include "nsCharTraits.h"

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

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


static const PRUint16 pairedChars[] = {
    0x0028, 0x0029, /* ascii paired punctuation */
    0x003c, 0x003e,
    0x005b, 0x005d,
    0x007b, 0x007d,
    0x00ab, 0x00bb, /* guillemets */
    0x2018, 0x2019, /* general punctuation */
    0x201c, 0x201d,
    0x2039, 0x203a,
    0x207d, 0x207e, /* superscripts and subscripts */
    0x208d, 0x208e,
    0x275b, 0x275c, /* dingbat quotes and brackets */
    0x275d, 0x275e,
    0x2768, 0x2769,
    0x276a, 0x276b,
    0x276c, 0x276d,
    0x276e, 0x276f,
    0x2770, 0x2771,
    0x2772, 0x2773,
    0x2774, 0x2775,
    /* omitted: lots of potentially-paired math symbols */
    0x2e22, 0x2e23, /* supplemental punctuation */
    0x2e24, 0x2e25,
    0x2e26, 0x2e27,
    0x2e28, 0x2e29,
    0x3008, 0x3009, /* chinese paired punctuation */
    0x300a, 0x300b,
    0x300c, 0x300d,
    0x300e, 0x300f,
    0x3010, 0x3011,
    0x3014, 0x3015,
    0x3016, 0x3017,
    0x3018, 0x3019,
    0x301a, 0x301b,
    0xfe59, 0xfe5a, /* small form variants */
    0xfe5b, 0xfe5c,
    0xfe5d, 0xfe5e,
    0xfe64, 0xfe65,
    0xff08, 0xff09, /* half-width and full-width forms */
    0xff1c, 0xff1e,
    0xff3b, 0xff3d,
    0xff5b, 0xff5d,
    0xff5f, 0xff60,
    0xff62, 0xff63
};

void
gfxScriptItemizer::push(PRInt32 pairIndex, PRInt32 scriptCode)
{
    pushCount  = LIMIT_INC(pushCount);
    fixupCount = LIMIT_INC(fixupCount);

    parenSP = INC1(parenSP);
    parenStack[parenSP].pairIndex  = pairIndex;
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
gfxScriptItemizer::fixup(PRInt32 scriptCode)
{
    PRInt32 fixupSP = DEC(parenSP, fixupCount);

    while (fixupCount-- > 0) {
        fixupSP = INC1(fixupSP);
        parenStack[fixupSP].scriptCode = scriptCode;
    }
}

static PRInt32
getPairIndex(PRUint32 ch)
{
    PRInt32 pairedCharCount = ARRAY_SIZE(pairedChars);
    PRInt32 pairedCharPower = mozilla::FindHighestBit(pairedCharCount);
    PRInt32 pairedCharExtra = pairedCharCount - pairedCharPower;

    PRInt32 probe = pairedCharPower;
    PRInt32 pairIndex = 0;

    if (ch >= pairedChars[pairedCharExtra]) {
        pairIndex = pairedCharExtra;
    }

    while (probe > 1) {
        probe >>= 1;

        if (ch >= pairedChars[pairIndex + probe]) {
            pairIndex += probe;
        }
    }

    if (pairedChars[pairIndex] != ch) {
        pairIndex = -1;
    }

    return pairIndex;
}

static bool
sameScript(PRInt32 runScript, PRInt32 currCharScript)
{
    return runScript <= MOZ_SCRIPT_INHERITED ||
           currCharScript <= MOZ_SCRIPT_INHERITED ||
           currCharScript == runScript;
}

gfxScriptItemizer::gfxScriptItemizer(const PRUnichar *src, PRUint32 length)
    : textPtr(src), textLength(length)
{
    reset();
}

void
gfxScriptItemizer::SetText(const PRUnichar *src, PRUint32 length)
{
    textPtr  = src;
    textLength = length;

    reset();
}

bool
gfxScriptItemizer::Next(PRUint32& aRunStart, PRUint32& aRunLimit,
                        PRInt32& aRunScript)
{
    /* if we've fallen off the end of the text, we're done */
    if (scriptLimit >= textLength) {
        return false;
    }

    SYNC_FIXUP();
    scriptCode = MOZ_SCRIPT_COMMON;

    for (scriptStart = scriptLimit; scriptLimit < textLength; scriptLimit += 1) {
        PRUint32 ch;
        PRInt32 sc;
        PRInt32 pairIndex;
        PRUint32 startOfChar = scriptLimit;

        ch = textPtr[scriptLimit];

        /*
         * MODIFICATION for Gecko - clear the paired-character stack
         * when we see a space character, because we cannot trust
         * context outside the current "word" when doing textrun
         * construction
         */
        if (ch == 0x20) {
            while (STACK_IS_NOT_EMPTY()) {
                pop();
            }
            sc = MOZ_SCRIPT_COMMON;
            pairIndex = -1;
        } else {
            /* decode UTF-16 (may be surrogate pair) */
            if (NS_IS_HIGH_SURROGATE(ch) && scriptLimit < textLength - 1) {
                PRUint32 low = textPtr[scriptLimit + 1];
                if (NS_IS_LOW_SURROGATE(low)) {
                    ch = SURROGATE_TO_UCS4(ch, low);
                    scriptLimit += 1;
                }
            }

            sc = mozilla::unicode::GetScriptCode(ch);

            pairIndex = getPairIndex(ch);

            /*
             * Paired character handling:
             *
             * if it's an open character, push it onto the stack.
             * if it's a close character, find the matching open on the
             * stack, and use that script code. Any non-matching open
             * characters above it on the stack will be poped.
             */
            if (pairIndex >= 0) {
                if ((pairIndex & 1) == 0) {
                    push(pairIndex, scriptCode);
                } else {
                    PRInt32 pi = pairIndex & ~1;

                    while (STACK_IS_NOT_EMPTY() && TOP().pairIndex != pi) {
                        pop();
                    }

                    if (STACK_IS_NOT_EMPTY()) {
                        sc = TOP().scriptCode;
                    }
                }
            }
        }

        if (sameScript(scriptCode, sc)) {
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
            if (pairIndex >= 0 && (pairIndex & 1) != 0) {
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
