/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdarg.h>

#include "nsCanvasRenderingContextGL.h"

#include "nsGLPbuffer.h"

#if 0
#include <xmmintrin.h>
#endif

void *nsGLPbuffer::sCurrentContextToken = nsnull;

void
nsGLPbuffer::LogMessage (const nsCString& errorString)
{
    if (mPriv)
        mPriv->LogMessage(errorString);
    else
        fprintf(stderr, "nsGLPbuffer: %s\n", errorString.get());
}

void
nsGLPbuffer::LogMessagef (const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[256];

    vsnprintf(buf, 256, fmt, ap);

    if (mPriv)
        mPriv->LogMessage(nsDependentCString(buf));
    else
        fprintf(stderr, "nsGLPbuffer: %s\n", buf);

    va_end(ap);
}

static void
premultiply_slow(unsigned char *src, unsigned int len)
{
    int a,t;
    for (unsigned int i=0; i<len; i+=4) {
        a = src[i+3];
        t = src[i]*a+0x80;
        src[i] = ((t>>8) + t) >> 8;
        t = src[i+1]*a+0x80;
        src[i+1] = ((t>>8) + t) >> 8;
        t = src[i+2]*a+0x80;
        src[i+2] = ((t>>8) + t) >> 8;
    }
}

#if 0
static void
premultiply_sse2(__m128i* block, __m128i* block_end)
{
    __m128i xmm0080, xmm0101, xmmAlpha, data, dataLo, dataHi, alphaLo, alphaHi;
    while (block < block_end) {
        xmm0080 = _mm_set1_epi16(0x0080);
        xmm0101 = _mm_set1_epi16(0x0101);
        xmmAlpha = _mm_set_epi32(0x00ff0000, 0x00000000, 0x00ff0000, 0x00000000);

        data = _mm_loadu_si128(block);
        dataLo = _mm_unpacklo_epi8 (data, _mm_setzero_si128 ());
        dataHi = _mm_unpackhi_epi8 (data, _mm_setzero_si128 ());

        alphaLo = _mm_shufflelo_epi16 (dataLo, _MM_SHUFFLE(3, 3, 3, 3));
        alphaHi = _mm_shufflelo_epi16 (dataHi, _MM_SHUFFLE(3, 3, 3, 3));
        alphaLo = _mm_shufflehi_epi16 (alphaLo, _MM_SHUFFLE(3, 3, 3, 3));
        alphaHi = _mm_shufflehi_epi16 (alphaHi, _MM_SHUFFLE(3, 3, 3, 3));

        alphaLo = _mm_or_si128(alphaLo, xmmAlpha);
        alphaHi = _mm_or_si128(alphaHi, xmmAlpha);

        dataLo = _mm_shufflelo_epi16 (dataLo, _MM_SHUFFLE(3, 2, 1, 0));
        dataLo = _mm_shufflehi_epi16 (dataLo, _MM_SHUFFLE(3, 2, 1, 0));
        dataHi = _mm_shufflelo_epi16 (dataHi, _MM_SHUFFLE(3, 2, 1, 0));
        dataHi = _mm_shufflehi_epi16 (dataHi, _MM_SHUFFLE(3, 2, 1, 0));

        dataLo = _mm_mullo_epi16(dataLo, alphaLo);
        dataHi = _mm_mullo_epi16(dataHi, alphaHi);

        dataLo = _mm_adds_epu16(dataLo, xmm0080);
        dataHi = _mm_adds_epu16(dataHi, xmm0080);

        dataLo = _mm_mulhi_epu16(dataLo, xmm0101);
        dataHi = _mm_mulhi_epu16(dataHi, xmm0101);

        data = _mm_packus_epi16 (dataLo, dataHi);
        _mm_storeu_si128(block, data);

        ++block;
    }
}
#endif

void
nsGLPbuffer::Premultiply(unsigned char *src, unsigned int len)
{
#if 0
    if (can_sse2) {
        premultiply_sse2((__m128i*)src, (__m128i*)(src+len));
        return;
    }
#endif

    premultiply_slow(src, len);
}
