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

#include "nsIPluginStreamListener.h"
#include "nsXPIDLPluginStreamListener.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginStreamListener, nsIPluginStreamListener)

nsXPIDLPluginStreamListener::nsXPIDLPluginStreamListener()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginStreamListener::nsXPIDLPluginStreamListener( nsIXPIDLPluginStreamListener *pluginStreamListener )
{
  NS_INIT_ISUPPORTS();
  this->pluginStreamListener = pluginStreamListener;
  NS_ADDREF( pluginStreamListener );
}

nsXPIDLPluginStreamListener::~nsXPIDLPluginStreamListener()
{
  NS_RELEASE( pluginStreamListener );
}

NS_IMETHODIMP
nsXPIDLPluginStreamListener::GetStreamType( nsPluginStreamType *result )
{
    return pluginStreamListener->GetStreamType( (PRUint32 *)result );
}

NS_IMETHODIMP
nsXPIDLPluginStreamListener::OnDataAvailable( nsIPluginStreamInfo *streamInfo,
                                              nsIInputStream *input,
                                              PRUint32 length )
{
    nsIXPIDLPluginStreamInfo *sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
    nsIXPIDLInputStream *iStream = new nsXPIDLInputStream( input );
    return pluginStreamListener->OnDataAvailable( sInfo, iStream, length );
}

NS_IMETHODIMP
nsXPIDLPluginStreamListener::OnFileAvailable( nsIPluginStreamInfo *streamInfo,
                                              const char *fileName )
{
    nsIXPIDLPluginStreamInfo *sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
    return pluginStreamListener->OnFileAvailable( sInfo, (PRUnichar *)fileName );
}

NS_IMETHODIMP
nsXPIDLPluginStreamListener::OnStartBinding( nsIPluginStreamInfo *streamInfo )
{
    nsIXPIDLPluginStreamInfo *sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
    return pluginStreamListener->OnStartBinding( sInfo );
}

NS_IMETHODIMP
nsXPIDLPluginStreamListener::OnStopBinding( nsIPluginStreamInfo *streamInfo,
                                            nsresult status )
{
    nsIXPIDLPluginStreamInfo *sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
    return pluginStreamListener->OnStopBinding( sInfo, (PRInt32)status );
}
