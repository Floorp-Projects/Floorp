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

#include "nsXPIDLPluginTagInfo.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPluginTagInfo, nsIXPIDLPluginTagInfo)

nsXPIDLPluginTagInfo::nsXPIDLPluginTagInfo()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPluginTagInfo::nsXPIDLPluginTagInfo( nsIPluginTagInfo *tagInfo )
{
  NS_INIT_ISUPPORTS();
  this->tagInfo = tagInfo;
  NS_ADDREF( tagInfo );
}

nsXPIDLPluginTagInfo::~nsXPIDLPluginTagInfo()
{
  NS_RELEASE( tagInfo );
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo::GetAttribute( const char *name, char **_retval )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXPIDLPluginTagInfo::GetAttributes( PRUint32 *count,
                                     char ***names,
                                     char ***values )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
