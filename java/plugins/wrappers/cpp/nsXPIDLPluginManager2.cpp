/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Serge Pikalev <sep@sparc.spb.su>
 */

#include "nsXPIDLPluginManager2.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginManager2, nsIXPIDLPluginManager2)

nsXPIDLPluginManager2::nsXPIDLPluginManager2()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginManager2::nsXPIDLPluginManager2( nsIPluginManager2 *pluginManager )
{
  NS_INIT_ISUPPORTS();
  this->pluginManager = pluginManager;
  NS_ADDREF( pluginManager );
}

nsXPIDLPluginManager2::~nsXPIDLPluginManager2()
{
  NS_RELEASE( pluginManager );
}

NS_IMETHODIMP
nsXPIDLPluginManager2::BeginWaitCursor()
{
    return pluginManager->BeginWaitCursor();
}

NS_IMETHODIMP
nsXPIDLPluginManager2::EndWaitCursor()
{
    return pluginManager->EndWaitCursor();
}

NS_IMETHODIMP
nsXPIDLPluginManager2::FindProxyForURL( const char *url, char **_retval )
{
    return pluginManager->FindProxyForURL( url, _retval );
}

NS_IMETHODIMP
nsXPIDLPluginManager2::SupportsURLProtocol( const char *protocol,
                                            PRBool *_retval )
{
    return pluginManager->SupportsURLProtocol( protocol, _retval );
}

// from nsXPIDLPluginManager

NS_IMETHODIMP
nsXPIDLPluginManager2::GetURL( nsISupports *pliginInstance,
                               const char *url,
                               const char *target,
                               nsIXPIDLPluginStreamListener *streamListener,
                               const char *altHost,
                               const char *referrer,
                               PRBool forceJSEnabled )
{
    return nsXPIDLPluginManager::GetURL( pliginInstance,
                                         url,
                                         target,
                                         streamListener,
                                         altHost,
                                         referrer,
                                         forceJSEnabled );
}

NS_IMETHODIMP
nsXPIDLPluginManager2::PostURL( nsISupports *pliginInstance,
                                const char *url,
                                PRUint32 postDataLength,
                                PRUint8 *postData,
                                PRUint32 postHeadersLength,
                                const char *postHeaders,
                                PRBool isFile,
                                const char *target,
                                nsIXPIDLPluginStreamListener *streamListener,
                                const char *altHost,
                                const char *referrer,
                                PRBool forceJSEnabled )
{
    return nsXPIDLPluginManager::PostURL( pliginInstance,
                                          url,
                                          postDataLength,
                                          postData,
                                          postHeadersLength,
                                          postHeaders,
                                          isFile,
                                          target,
                                          streamListener,
                                          altHost,
                                          referrer,
                                          forceJSEnabled );
}

NS_IMETHODIMP
nsXPIDLPluginManager2::ReloadPlugins( PRBool reloadPages )
{
    return nsXPIDLPluginManager::ReloadPlugins( reloadPages );
}

NS_IMETHODIMP
nsXPIDLPluginManager2::UserAgent( char **_retval )
{
    return nsXPIDLPluginManager::UserAgent( _retval );
}
