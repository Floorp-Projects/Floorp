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

#include "nsIPlugin.h"
#include "nsXPIDLPlugin.h"

NS_IMPL_ISUPPORTS1(nsXPIDLPlugin, nsIPlugin)

nsXPIDLPlugin::nsXPIDLPlugin()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLPlugin::nsXPIDLPlugin( nsIXPIDLPlugin *plugin )
{
  NS_INIT_ISUPPORTS();
  this->plugin = plugin;
  NS_ADDREF( plugin );
}

nsXPIDLPlugin::~nsXPIDLPlugin()
{
  NS_RELEASE( plugin );
}

NS_IMETHODIMP
nsXPIDLPlugin::CreatePluginInstance( nsISupports *aOuter,
                      REFNSIID aIID,
                      const char* aPluginMIMEType,
                      void **aResult )
{
    return plugin->CreatePluginInstance( aOuter, aIID, aPluginMIMEType, aResult );
}

NS_IMETHODIMP
nsXPIDLPlugin::Initialize()
{
    return plugin->Initialize();
}

NS_IMETHODIMP
nsXPIDLPlugin::Shutdown()
{
    return plugin->Shutdown();
}
