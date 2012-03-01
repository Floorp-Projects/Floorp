/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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
 *
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

#ifndef GFX_SCRIPTITEMIZER_H
#define GFX_SCRIPTITEMIZER_H

#include "mozilla/StandardInteger.h"
#include "prtypes.h"
#include "harfbuzz/hb.h"
#include "nsUnicodeScriptCodes.h"

#define PAREN_STACK_DEPTH 32

class gfxScriptItemizer
{
public:
    gfxScriptItemizer(const PRUnichar *src, PRUint32 length);

    void SetText(const PRUnichar *src, PRUint32 length);

    bool Next(PRUint32& aRunStart, PRUint32& aRunLimit,
              PRInt32& aRunScript);

protected:
    void reset() {
        scriptStart = 0;
        scriptLimit = 0;
        scriptCode  = MOZ_SCRIPT_INVALID;
        parenSP     = -1;
        pushCount   =  0;
        fixupCount  =  0;
    }

    void push(PRInt32 pairIndex, PRInt32 scriptCode);
    void pop();
    void fixup(PRInt32 scriptCode);

    struct ParenStackEntry {
        PRInt32 pairIndex;
        PRInt32 scriptCode;
    };

    const PRUnichar *textPtr;
    PRUint32 textLength;

    PRUint32 scriptStart;
    PRUint32 scriptLimit;
    PRInt32  scriptCode;

    struct ParenStackEntry parenStack[PAREN_STACK_DEPTH];
    PRUint32 parenSP;
    PRUint32 pushCount;
    PRUint32 fixupCount;
};

#endif /* GFX_SCRIPTITEMIZER_H */
