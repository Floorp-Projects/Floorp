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

#include "nsXPIDLPluginTagInfo2.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginTagInfo2, nsIXPIDLPluginTagInfo2)

nsXPIDLPluginTagInfo2::nsXPIDLPluginTagInfo2()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginTagInfo2::nsXPIDLPluginTagInfo2( nsIPluginTagInfo2 *tagInfo )
{
  NS_INIT_ISUPPORTS();
  this->tagInfo = tagInfo;
  NS_ADDREF( tagInfo );
}

nsXPIDLPluginTagInfo2::~nsXPIDLPluginTagInfo2()
{
  NS_RELEASE( tagInfo );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetAlignment( char * *aAlignment )
{
    return tagInfo->GetAlignment( (const char **)aAlignment );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetParameter( const char *name, char **_retval )
{
    return tagInfo->GetParameter( name, (const char **)_retval );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetParameters( PRUint32 *count,
                                      char ***names,
                                      char ***values )
{
    return tagInfo->GetParameters( (PRUint16)*count,
                                   (const char*const*)*names,
                                   (const char*const*)*values );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetBorderHorizSpace( PRUint32 *aBorderHorizSpace )
{
    return tagInfo->GetBorderHorizSpace( aBorderHorizSpace );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetBorderVertSpace( PRUint32 *aBorderVertSpace )
{
    return tagInfo->GetBorderVertSpace( aBorderVertSpace );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetDocumentBase( char * *aDocumentBase )
{
    return tagInfo->GetDocumentBase( (const char **)aDocumentBase );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetDocumentEncoding( char * *aDocumentEncoding )
{
    return tagInfo->GetDocumentEncoding( (const char**)aDocumentEncoding );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetHeight( PRUint32 *aHeight )
{
    return tagInfo->GetHeight( aHeight );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetWidth( PRUint32 *aWidth )
{
    return tagInfo->GetWidth( aWidth );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetTagText( char * *aTagText )
{
    return tagInfo->GetTagText( (const char **)aTagText );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetTagType( char * *aTagType )
{
    return tagInfo->GetTagType( (nsPluginTagType *)aTagType );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetUniqueID( PRUint32 *aUniqueID )
{
    return tagInfo->GetUniqueID( aUniqueID );
}

// from TagInfo
NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetAttribute( const char *name, char **_retval )
{
    return nsXPIDLPluginTagInfo::GetAttribute( name, _retval );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo2::GetAttributes( PRUint32 *count,
                                      char ***names,
                                      char ***values )
{
    return nsXPIDLPluginTagInfo::GetAttributes( count, names, values );
}
