/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:expandtab:ts=4 sw=4:
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by Oracle Corporation are  Copyright (C) 2004
 * by Oracle Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@off.net> (original author)
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

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "nsWebDAVInternal.h"

#include "nsIWebDAVService.h"
#include "nsWebDAVServiceCID.h"

#include "nsIServiceManagerUtils.h"

#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"

#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOM3Node.h"

#include "nsIDOMParser.h"

#include "nsIGenericFactory.h"

class nsWebDAVService : public nsIWebDAVService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBDAVSERVICE
    
    nsWebDAVService();
    virtual ~nsWebDAVService();
protected:
    nsresult EnsureIOService();
    nsresult ChannelFromResource(nsIWebDAVResource *resource,
                                 nsIHttpChannel** channel);
    nsCOMPtr<nsIIOService> mIOService; // XXX weak?
};

NS_IMPL_ISUPPORTS1_CI(nsWebDAVService, nsIWebDAVService)

#define ENSURE_IO_SERVICE()                     \
{                                               \
    nsresult rv_ = EnsureIOService();           \
    NS_ENSURE_SUCCESS(rv_, rv_);                \
}

nsresult
nsWebDAVService::EnsureIOService()
{
    if (!mIOService) {
        nsresult rv;
        mIOService = do_GetIOService(&rv);
        if (!mIOService)
            return rv;
    }

    return NS_OK;
}

nsresult
nsWebDAVService::ChannelFromResource(nsIWebDAVResource *resource,
                                     nsIHttpChannel **channel)
{
    ENSURE_IO_SERVICE();

    nsCAutoString spec;
    nsresult rv = resource->GetUrlSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    if (spec.IsEmpty()) {
        NS_ASSERTION(0, "non-empty spec!");
        return NS_ERROR_INVALID_ARG;
    }

    nsCOMPtr<nsIURI> uri;
    rv = mIOService->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> baseChannel;
    rv = mIOService->NewChannelFromURI(uri, getter_AddRefs(baseChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(baseChannel, channel);
}

nsWebDAVService::nsWebDAVService()
{
    gDAVLog = PR_NewLogModule("webdav");
}

nsWebDAVService::~nsWebDAVService()
{
  /* destructor code */
}

NS_IMETHODIMP
nsWebDAVService::LockResources(PRUint32 count, nsIWebDAVResource **resources,
                               nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::UnlockResources(PRUint32 count, nsIWebDAVResource **resources,
                                 nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::GetResourcePropertyNames(nsIWebDAVResource *resource,
                                          PRBool withDepth,
                                          nsIWebDAVMetadataListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsWebDAVService::GetResourceProperties(nsIWebDAVResource *resource,
                                       PRUint32 propCount,
                                       const char **properties,
                                       PRBool withDepth,
                                       nsIWebDAVMetadataListener *listener)
{
    nsresult rv;

    NS_ENSURE_ARG(resource);
    NS_ENSURE_ARG(listener);

    nsCOMPtr<nsIHttpChannel> channel;
    rv = ChannelFromResource(resource, getter_AddRefs(channel));
    if (NS_FAILED(rv))
        return rv;

    // XXX I wonder how many compilers this will break...
    const nsACString &depthValue = withDepth ? NS_LITERAL_CSTRING("1") :
        NS_LITERAL_CSTRING("0");
    channel->SetRequestMethod(NS_LITERAL_CSTRING("PROPFIND"));
    channel->SetRequestHeader(NS_LITERAL_CSTRING("Depth"), depthValue, false);

    if (LOG_ENABLED()) {
        nsCAutoString spec;
        resource->GetUrlSpec(spec);
        LOG(("PROPFIND starting for %s", spec.get()));
    }

    nsCOMPtr<nsIStreamListener> streamListener = 
        NS_NewPropfindStreamListener(resource, listener);

    if (!streamListener)
        return NS_ERROR_OUT_OF_MEMORY;

    return channel->AsyncOpen(streamListener, channel);
}

NS_IMETHODIMP
nsWebDAVService::GetResourceOptions(nsIWebDAVResource *resource,
                                    nsIWebDAVMetadataListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::GetChildren(nsIWebDAVResource *resource, PRUint32 depth,
                             nsIWebDAVChildrenListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::Get(nsIWebDAVResource *resource, nsIStreamListener *listener)
{
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> channel;
    rv = ChannelFromResource(resource, getter_AddRefs(channel));
    if (NS_FAILED(rv))
        return rv;

    return channel->AsyncOpen(listener, nsnull);
}

NS_IMETHODIMP
nsWebDAVService::Put(nsIWebDAVResourceWithData *resource,
                     nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::Remove(nsIWebDAVResource *resource,
                        nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::MoveTo(nsIWebDAVResourceWithTarget *resource,
                        nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::CopyTo(nsIWebDAVResourceWithTarget *resource,
                        nsIWebDAVOperationListener *listener, PRBool recursive)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebDAVService)

NS_DECL_CLASSINFO(nsWebDAVService)

static const nsModuleComponentInfo components[] =
{
    { "WebDAV Service", NS_WEBDAVSERVICE_CID, NS_WEBDAVSERVICE_CONTRACTID,
      nsWebDAVServiceConstructor,
      NULL, NULL, NULL,
      NS_CI_INTERFACE_GETTER_NAME(nsWebDAVService),
      NULL,
      &NS_CLASSINFO_NAME(nsWebDAVService)
    }
};

NS_IMPL_NSGETMODULE(nsWebDAVService, components)
