/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistSerializeChild.h"

#include <algorithm>

#include "nsThreadUtils.h"
#include "ipc/IPCMessageUtils.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(WebBrowserPersistSerializeChild,
                  nsIWebBrowserPersistWriteCompletion,
                  nsIWebBrowserPersistURIMap,
                  nsIOutputStream)

WebBrowserPersistSerializeChild::WebBrowserPersistSerializeChild(const WebBrowserPersistURIMap& aMap)
: mMap(aMap)
{
}

WebBrowserPersistSerializeChild::~WebBrowserPersistSerializeChild()
{
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::OnFinish(nsIWebBrowserPersistDocument* aDocument,
                                          nsIOutputStream* aStream,
                                          const nsACString& aContentType,
                                          nsresult aStatus)
{
    MOZ_ASSERT(aStream == this);
    nsCString contentType(aContentType);
    Send__delete__(this, contentType, aStatus);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::GetNumMappedURIs(uint32_t* aNum)
{
    *aNum = static_cast<uint32_t>(mMap.mapURIs().Length());
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::GetURIMapping(uint32_t aIndex,
                                               nsACString& aMapFrom,
                                               nsACString& aMapTo)
{
    if (aIndex >= mMap.mapURIs().Length()) {
        return NS_ERROR_INVALID_ARG;
    }
    aMapFrom = mMap.mapURIs()[aIndex].mapFrom();
    aMapTo = mMap.mapURIs()[aIndex].mapTo();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::GetTargetBaseURI(nsACString& aURI)
{
    aURI = mMap.targetBaseURI();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::Close()
{
    NS_WARNING("WebBrowserPersistSerializeChild::Close()");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::Flush()
{
    NS_WARNING("WebBrowserPersistSerializeChild::Flush()");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::Write(const char* aBuf, uint32_t aCount,
                                       uint32_t* aWritten)
{
    // Normally an nsIOutputStream would have to be thread-safe, but
    // nsDocumentEncoder currently doesn't call this off the main
    // thread (which also means it's difficult to test the
    // thread-safety code this class doesn't yet have).
    //
    // This is *not* an NS_ERROR_NOT_IMPLEMENTED, because at this
    // point we've probably already misused the non-thread-safe
    // refcounting.
    MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Fix this class to be thread-safe.");

    // Work around bug 1181433 by sending multiple messages if
    // necessary to write the entire aCount bytes, even though
    // nsIOutputStream.idl says we're allowed to do a short write.
    const char* buf = aBuf;
    uint32_t count = aCount;
    *aWritten = 0;
    while (count > 0) {
        uint32_t toWrite = std::min(IPC::MAX_MESSAGE_SIZE, count);
        nsTArray<uint8_t> arrayBuf;
        // It would be nice if this extra copy could be avoided.
        arrayBuf.AppendElements(buf, toWrite);
        SendWriteData(Move(arrayBuf));
        *aWritten += toWrite;
        buf += toWrite;
        count -= toWrite;
    }
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::WriteFrom(nsIInputStream* aFrom,
                                           uint32_t aCount,
                                           uint32_t* aWritten)
{
    NS_WARNING("WebBrowserPersistSerializeChild::WriteFrom()");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::WriteSegments(nsReadSegmentFun aFun,
                                               void* aCtx,
                                               uint32_t aCount,
                                               uint32_t* aWritten)
{
    NS_WARNING("WebBrowserPersistSerializeChild::WriteSegments()");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserPersistSerializeChild::IsNonBlocking(bool* aNonBlocking)
{
    // Writes will never fail with NS_BASE_STREAM_WOULD_BLOCK, so:
    *aNonBlocking = false;
    return NS_OK;
}

} // namespace mozilla
