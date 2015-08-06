/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistResourcesChild_h__
#define WebBrowserPersistResourcesChild_h__

#include "mozilla/PWebBrowserPersistResourcesChild.h"

#include "nsIWebBrowserPersistDocument.h"

namespace mozilla {

class WebBrowserPersistResourcesChild final
    : public PWebBrowserPersistResourcesChild
    , public nsIWebBrowserPersistResourceVisitor
{
public:
    WebBrowserPersistResourcesChild();

    NS_DECL_NSIWEBBROWSERPERSISTRESOURCEVISITOR
    NS_DECL_ISUPPORTS
private:
    virtual ~WebBrowserPersistResourcesChild();
};

} // namespace mozilla

#endif // WebBrowserPersistDocumentChild_h__
