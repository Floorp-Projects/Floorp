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

#include "nsIPluginInstancePeer.h"
#include "nsXPIDLPluginInstancePeer.h"

#include "nsIOutputStream.h"
#include "nsXPIDLOutputStream.h"
#include "nsIPluginTagInfo2.h"
#include "nsXPIDLPluginTagInfo2.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginInstancePeer, nsIXPIDLPluginInstancePeer)

nsXPIDLPluginInstancePeer::nsXPIDLPluginInstancePeer()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginInstancePeer::nsXPIDLPluginInstancePeer( nsIPluginInstancePeer *pluginInstancePeer )
{
  NS_INIT_ISUPPORTS();
  this->pluginInstancePeer = pluginInstancePeer;
  NS_ADDREF( pluginInstancePeer );
}

nsXPIDLPluginInstancePeer::~nsXPIDLPluginInstancePeer()
{
  NS_RELEASE( pluginInstancePeer );
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::GetMIMEType( char * *aMIMEType )
{
    nsMIMEType *mimeType = (nsMIMEType *)aMIMEType;
    return pluginInstancePeer->GetMIMEType( mimeType );
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::GetMode( PRUint32 *aMode )
{
    nsPluginMode *mode = (nsPluginMode *)aMode;
    return pluginInstancePeer->GetMode( mode );
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::GetTagInfo( nsIXPIDLPluginTagInfo2 * *aTagInfo )
{
    nsIPluginTagInfo2 *tagInfo;
    nsresult res = pluginInstancePeer->QueryInterface( NS_GET_IID(nsIPluginTagInfo2), (void **)&tagInfo );
    if( NS_FAILED(res) ) {
        *aTagInfo = NULL;
        return res;
    }
    *aTagInfo = new nsXPIDLPluginTagInfo2( tagInfo );
    return res;
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::GetValue( PRInt32 variable, char **_retval )
{
    nsPluginInstancePeerVariable var = (nsPluginInstancePeerVariable)variable;
    return pluginInstancePeer->GetValue( var, (void *)*_retval );
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::NewStream( const char *type,
                                      const char *target,
                                      nsIXPIDLOutputStream **_retval )
{
    nsIOutputStream **oStream;
    nsresult nsRes = pluginInstancePeer->NewStream( type, target, oStream );
    *_retval = new nsXPIDLOutputStream( *oStream );
    return nsRes;
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::SetWindowSize( PRUint32 width, PRUint32 height )
{
    return pluginInstancePeer->SetWindowSize( width, height );
}

NS_IMETHODIMP
nsXPIDLPluginInstancePeer::ShowStatus( const char *message )
{
    return pluginInstancePeer->ShowStatus( message );
}
