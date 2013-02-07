/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"

#include "js/CharacterEncoding.h"

#include "jscntxtinlines.h"
#include "jsscriptinlines.h"

using namespace JS;

Latin1CharsZ
JS::LossyTwoByteCharsToNewLatin1CharsZ(JSContext *cx, TwoByteChars tbchars)
{
    AutoAssertNoGC nogc;
    JS_ASSERT(cx);
    size_t len = tbchars.length();
    unsigned char *latin1 = cx->pod_malloc<unsigned char>(len + 1);
    if (!latin1)
        return Latin1CharsZ();
    for (size_t i = 0; i < len; ++i)
        latin1[i] = static_cast<unsigned char>(tbchars[i]);
    latin1[len] = '\0';
    return Latin1CharsZ(latin1, len);
}

