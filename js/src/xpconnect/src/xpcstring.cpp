/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * Infrastructure for sharing DOMString data with JSStrings.
 *
 * Importing an nsAReadableString into JS:
 * If possible (GetSharedBufferHandle works) use the external string support in
 * JS to create a JSString that points to the readable's buffer.  We keep a
 * reference to the buffer handle until the JSString is finalized.
 *
 * Exporting a JSString as an nsAReadable:
 * Wrap the JSString with a root-holding XPCJSReadableStringWrapper, which roots
 * the string and exposes its buffer via the nsAReadableString interface, as
 * well as providing refcounting support.
 */

#include "xpcprivate.h"

/*
 * We require that STRING_TO_JSVAL(s) != (jsval)s, for tracking rootedness.
 * When we colonize Mars and change JSVAL_STRING, we'll need to update this
 * code.
 */
#if JSVAL_STRING == 0
#error "JSVAL_STRING has zero value -- need to fix root management!"
#endif

/* static */
const PRUnichar XPCReadableJSStringWrapper::sEmptyString = PRUnichar(0);

XPCReadableJSStringWrapper::~XPCReadableJSStringWrapper()
{
    if (mBufferHandle)
    {
        if (mHandleIsShared)
        {
            mBufferHandle->ReleaseReference();
        }
        else
        {
            delete mBufferHandle;
        }
    }
}

const nsSharedBufferHandle<PRUnichar>*
XPCReadableJSStringWrapper::BufferHandle(JSBool shared) const
{
    if (!mStr) {
        // This is a "void" string, no buffer handle available
        return nsnull;
    }

    XPCReadableJSStringWrapper * mutable_this =
        NS_CONST_CAST(XPCReadableJSStringWrapper *, this);

    if (!mBufferHandle)
    {
        mutable_this->mBufferHandle =
            new WrapperBufferHandle(mutable_this, mStr);

    }

    if (shared && !mHandleIsShared)
    {
        mutable_this->mBufferHandle->AcquireReference();
        mutable_this->mBufferHandle->mAllocator.RootString();
        mutable_this->mHandleIsShared = JS_TRUE;
    }

    return mBufferHandle;
}

void
XPCReadableJSStringWrapper::WrapperBufferHandle::Allocator::
Deallocate(PRUnichar *) const
{
    if (JSVAL_IS_STRING(mStr))
    {
        // unroot
        JSRuntime *rt;
        nsCOMPtr<nsIJSRuntimeService> rtsvc =
            dont_AddRef(NS_STATIC_CAST(nsIJSRuntimeService*,
                                     nsJSRuntimeServiceImpl::GetSingleton()));
        if (rtsvc && NS_SUCCEEDED(rtsvc->GetRuntime(&rt)))
        {
            JS_RemoveRootRT(rt,
                            NS_CONST_CAST(void **,
                                          NS_REINTERPRET_CAST(void * const *,
                                                          &mStr)));
            Allocator *mutable_this = NS_CONST_CAST(Allocator *, this);
            // store untagged to indicate that we're not rooted
            mutable_this->mStr = NS_REINTERPRET_CAST(jsval,
                                                     JSVAL_TO_STRING(mStr));
        }
        else
        {
            NS_ERROR("Unable to unroot mStr!  Prepare for leak or crash!");
        }
    }
}

JSBool
XPCReadableJSStringWrapper::WrapperBufferHandle::Allocator::RootString()
{
    JSRuntime *rt;
    nsCOMPtr<nsIJSRuntimeService> rtsvc =
        dont_AddRef(NS_STATIC_CAST(nsIJSRuntimeService*,
                                   nsJSRuntimeServiceImpl::GetSingleton()));
    JSBool ok = rtsvc &&
        NS_SUCCEEDED(rtsvc->GetRuntime(&rt)) &&
        JS_AddNamedRootRT(rt,
                          NS_CONST_CAST(void **,
                                        NS_REINTERPRET_CAST(void * const *,
                                                            &mStr)),
                          "WrapperBufferHandle.mAllocator.mStr");
    if (ok)
    {
        // Indicate that we've rooted the string by storing it as a
        // string-tagged jsval
        mStr = STRING_TO_JSVAL(NS_REINTERPRET_CAST(JSString *, mStr));
    }
    return ok;
}

// structure for entry in the DOMStringTable
struct SharedStringEntry : public JSDHashEntryHdr {
    void *key;
    const nsSharedBufferHandle<PRUnichar> *handle;
};

// table to match JSStrings to their handles at finalization time
static JSDHashTable DOMStringTable;
static intN DOMStringFinalizerIndex = -1;

// unref the appropriate handle when the matching string is finalized
JS_STATIC_DLL_CALLBACK(void)
FinalizeDOMString(JSContext *cx, JSString *str)
{
    NS_ASSERTION(DOMStringFinalizerIndex != -1,
                 "XPCConvert: DOM string finalizer called uninitialized!");

    SharedStringEntry *entry =
        NS_STATIC_CAST(SharedStringEntry *,
                       JS_DHashTableOperate(&DOMStringTable, str,
                                            JS_DHASH_LOOKUP));

    // entry might be empty if we ran out of memory adding it to the hash
    if (JS_DHASH_ENTRY_IS_BUSY(entry))
    {
        entry->handle->ReleaseReference();
        (void) JS_DHashTableOperate(&DOMStringTable, str, JS_DHASH_REMOVE);
    }
}

// initialize the aforementioned hash table and register our external finalizer
static JSBool
InitializeDOMStringFinalizer()
{
    if (!JS_DHashTableInit(&DOMStringTable, JS_DHashGetStubOps(), NULL,
                          sizeof(SharedStringEntry), JS_DHASH_MIN_SIZE))
    {
        return JS_FALSE;
    }

    DOMStringFinalizerIndex =
        JS_AddExternalStringFinalizer(FinalizeDOMString);

    if (DOMStringFinalizerIndex == -1)
    {
        JS_DHashTableFinish(&DOMStringTable);
        return JS_FALSE;
    }

    return JS_TRUE;
}

// cleanup enumerator for the DOMStringTable
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
ReleaseHandleAndRemove(JSDHashTable *table, JSDHashEntryHdr *hdr,
                       uint32 number, void *arg)
{
    SharedStringEntry *entry = NS_STATIC_CAST(SharedStringEntry *, hdr);
    entry->handle->ReleaseReference();
    return JS_DHASH_REMOVE;
}

// clean up the finalizer infrastructure, releasing all outstanding handles
// static
void
XPCStringConvert::ShutdownDOMStringFinalizer()
{
    if (DOMStringFinalizerIndex == -1)
    {
        return;
    }

    // enumerate and release
    (void)JS_DHashTableEnumerate(&DOMStringTable, ReleaseHandleAndRemove, NULL);

    JS_DHashTableFinish(&DOMStringTable);
    DOMStringFinalizerIndex =
        JS_RemoveExternalStringFinalizer(FinalizeDOMString);

    DOMStringFinalizerIndex = -1;
}

// convert a readable to a JSString, sharing if possible and copying otherwise
// static
JSString *
XPCStringConvert::ReadableToJSString(JSContext *cx,
                                     const nsAReadableString &readable)
{
    const nsSharedBufferHandle<PRUnichar> *handle;
    handle = readable.GetSharedBufferHandle();

    JSString *str;
    if (!handle || NS_PTR_TO_INT32(handle) == 1)
    {
        // blech, have to copy.
        PRUint32 length = readable.Length();
        jschar *chars = NS_REINTERPRET_CAST(jschar *,
                                            JS_malloc(cx, (length + 1) *
                                                      sizeof(jschar)));
        if (!chars)
            return NULL;

        if (length && !CopyUnicodeTo(readable, 0,
                                     NS_REINTERPRET_CAST(PRUnichar *, chars),
                                     length))
        {
            JS_free(cx, chars);
            return NULL;
        }

        chars[length] = 0;

        str = JS_NewUCString(cx, chars, length);
        if (!str)
            JS_free(cx, chars);

        return str;
    }

    if (DOMStringFinalizerIndex == -1 && !InitializeDOMStringFinalizer())
        return NULL;

    str = JS_NewExternalString(cx,
                               NS_CONST_CAST(jschar *,
                                             NS_REINTERPRET_CAST(const jschar *,
                                                          handle->DataStart())),
                                             handle->DataLength(),
                                             DOMStringFinalizerIndex);
    if (!str)
        return NULL;

    SharedStringEntry *entry =
        NS_STATIC_CAST(SharedStringEntry *,
                       JS_DHashTableOperate(&DOMStringTable, str,
                                            JS_DHASH_ADD));

    if (!entry)
        return NULL; // str will be cleaned up by GC

    entry->handle = handle;
    entry->key = str;
    handle->AcquireReference();
    return str;
}

// static
XPCReadableJSStringWrapper *
XPCStringConvert::JSStringToReadable(JSString *str)
{
    return new XPCReadableJSStringWrapper(str);
}
