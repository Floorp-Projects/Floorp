/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistSerializeChild_h__
#define WebBrowserPersistSerializeChild_h__

#include "mozilla/PWebBrowserPersistSerializeChild.h"

#include "mozilla/PWebBrowserPersistDocument.h"
#include "nsIWebBrowserPersistDocument.h"
#include "nsIOutputStream.h"

namespace mozilla {

class WebBrowserPersistSerializeChild final
    : public PWebBrowserPersistSerializeChild
    , public nsIWebBrowserPersistWriteCompletion
    , public nsIWebBrowserPersistURIMap
    , public nsIOutputStream
{
public:
    explicit WebBrowserPersistSerializeChild(const WebBrowserPersistURIMap& aMap);

    NS_DECL_NSIWEBBROWSERPERSISTWRITECOMPLETION
    NS_DECL_NSIWEBBROWSERPERSISTURIMAP
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_ISUPPORTS
private:
    WebBrowserPersistURIMap mMap;

    virtual ~WebBrowserPersistSerializeChild();
};

} // namespace mozilla

#endif // WebBrowserPersistSerializeChild_h__
