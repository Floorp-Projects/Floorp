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

#include "nsXPIDLPluginStreamInfo.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginStreamInfo, nsIXPIDLPluginStreamInfo)

nsXPIDLPluginStreamInfo::nsXPIDLPluginStreamInfo()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginStreamInfo::nsXPIDLPluginStreamInfo( nsIPluginStreamInfo *pluginStreamInfo )
{
  NS_INIT_ISUPPORTS();
  this->pluginStreamInfo = pluginStreamInfo;
  NS_ADDREF( pluginStreamInfo );
}

nsXPIDLPluginStreamInfo::~nsXPIDLPluginStreamInfo()
{
  NS_RELEASE( pluginStreamInfo );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::GetContentType( char * *aContentType )
{
    return pluginStreamInfo->GetContentType( (nsMIMEType *)aContentType );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::GetLastModified( PRUint32 *aLastModified )
{
    return pluginStreamInfo->GetLastModified( aLastModified );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::GetLength( PRUint32 *aLength )
{
    return pluginStreamInfo->GetLength( aLength );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::GetURL( PRUnichar * *aURL )
{
    return pluginStreamInfo->GetURL( (const char **)aURL );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::IsSeekable( PRBool *_retval )
{
    return pluginStreamInfo->IsSeekable( _retval );
}

NS_IMETHODIMP
nsXPIDLPluginStreamInfo::RequestRead( PRUint32 count,
                                      PRInt32 *offsets,
                                      PRUint32 *lengths )
{
    nsByteRange *rangeList, *last;

    // rekolbasing data to nsByteRange structure
    for( int i=0; i<count; i++ ) {
        nsByteRange *newRange = new nsByteRange();
        newRange->offset = offsets[i];
        newRange->length = lengths[i];
        newRange->next = NULL;
        if( i == 0 ) rangeList = last = newRange;
        else {
            last->next = newRange;
            last = last->next;
        }
    }

    // make a call
    return pluginStreamInfo->RequestRead( rangeList );
}
