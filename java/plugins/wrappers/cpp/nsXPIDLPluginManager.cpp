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

#include "nsIPluginManager.h"
#include "nsXPIDLPluginManager.h"
#include "nsXPIDLPluginManager2.h"

#include "nsXPIDLPluginStreamListener.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginManager, nsIXPIDLPluginManager)

nsXPIDLPluginManager::nsXPIDLPluginManager()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginManager::nsXPIDLPluginManager( nsIPluginManager *pluginManager )
{
  NS_INIT_ISUPPORTS();
  this->pluginManager = pluginManager;
  NS_ADDREF( pluginManager );
}

nsXPIDLPluginManager::~nsXPIDLPluginManager()
{
  NS_RELEASE( pluginManager );
}

NS_IMETHODIMP
nsXPIDLPluginManager::GetURL( nsISupports *pliginInstance,
                              const char *url,
                              const char *target,
                              nsIXPIDLPluginStreamListener *streamListener,
                              const char *altHost,
                              const char *referrer,
                              PRBool forceJSEnabled )
{
    nsIPluginStreamListener *sListener = new nsXPIDLPluginStreamListener( streamListener );
    return pluginManager->GetURL( pliginInstance,
                                  (char *)url,
                                  (char *)target,
                                  sListener,
                                  (char *)altHost,
                                  (char *)referrer,
                                  forceJSEnabled );
}

NS_IMETHODIMP
nsXPIDLPluginManager::PostURL( nsISupports *pliginInstance,
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
    nsIPluginStreamListener *sListener = new nsXPIDLPluginStreamListener( streamListener );
    return pluginManager->PostURL( pliginInstance,
                                   (char *)url,
                                   postDataLength,
                                   (char *)postData,
                                   isFile,
                                   (char *)target,
                                   sListener,
                                   (char *)altHost,
                                   (char *)referrer,
                                   forceJSEnabled,
                                   postHeadersLength,
                                   postHeaders );
}

NS_IMETHODIMP
nsXPIDLPluginManager::ReloadPlugins( PRBool reloadPages )
{
    return pluginManager->ReloadPlugins( reloadPages );
}

NS_IMETHODIMP
nsXPIDLPluginManager::UserAgent( char **_retval )
{
    return pluginManager->UserAgent( (const char **)_retval );
}
