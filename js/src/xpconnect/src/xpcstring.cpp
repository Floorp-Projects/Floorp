/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Infrastructure for sharing DOMString data with JSStrings.
 *
 * Importing an nsAString into JS:
 * If possible (GetSharedBufferHandle works) use the external string support in
 * JS to create a JSString that points to the readable's buffer.  We keep a
 * reference to the buffer handle until the JSString is finalized.
 *
 * Exporting a JSString as an nsAReadable:
 * Wrap the JSString with a root-holding XPCJSReadableStringWrapper, which roots
 * the string and exposes its buffer via the nsAString interface, as
 * well as providing refcounting support.
 */

#include "xpcprivate.h"
#include "nsStringBuffer.h"

static int sDOMStringFinalizerIndex = -1;

static void
DOMStringFinalizer(JSContext *cx, JSString *str)
{
    nsStringBuffer::FromData(JS_GetStringChars(str))->Release();
}

void
XPCStringConvert::ShutdownDOMStringFinalizer()
{
    if (sDOMStringFinalizerIndex == -1)
        return;

    JS_RemoveExternalStringFinalizer(DOMStringFinalizer);
    sDOMStringFinalizerIndex = -1;
}

// convert a readable to a JSString, copying string data
// static
jsval
XPCStringConvert::ReadableToJSVal(JSContext *cx,
                                  const nsAString &readable)
{
    JSString *str;

    PRUint32 length = readable.Length();

    JSAtom *atom;
    if (length == 0 && (atom = cx->runtime->atomState.emptyAtom))
    {
        NS_ASSERTION(ATOM_IS_STRING(atom), "What kind of atom is this?");
        return ATOM_KEY(atom);
    }

    nsStringBuffer *buf = nsStringBuffer::FromString(readable);
    if (buf)
    {
        // yay, we can share the string's buffer!

        if (sDOMStringFinalizerIndex == -1)
        {
            sDOMStringFinalizerIndex =
                    JS_AddExternalStringFinalizer(DOMStringFinalizer);
            if (sDOMStringFinalizerIndex == -1)
                return JSVAL_NULL;
        }

        str = JS_NewExternalString(cx, 
                                   reinterpret_cast<jschar *>(buf->Data()),
                                   length, sDOMStringFinalizerIndex);

        if (str)
            buf->AddRef();
    }
    else
    {
        // blech, have to copy.

        jschar *chars = reinterpret_cast<jschar *>
                                        (JS_malloc(cx, (length + 1) *
                                                      sizeof(jschar)));
        if (!chars)
            return JSVAL_NULL;

        if (length && !CopyUnicodeTo(readable, 0,
                                     reinterpret_cast<PRUnichar *>(chars),
                                     length))
        {
            JS_free(cx, chars);
            return JSVAL_NULL;
        }

        chars[length] = 0;

        str = JS_NewUCString(cx, chars, length);
        if (!str)
            JS_free(cx, chars);
    }
    return STRING_TO_JSVAL(str);
}

// static
XPCReadableJSStringWrapper *
XPCStringConvert::JSStringToReadable(XPCCallContext& ccx, JSString *str)
{
    return ccx.NewStringWrapper(reinterpret_cast<PRUnichar *>(JS_GetStringChars(str)),
                                JS_GetStringLength(str));
}
