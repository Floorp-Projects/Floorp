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

#ifndef _nsXPIDLPlugin_included_
#define _nsXPIDLPlugin_included_

#include "nsIPlugin.h"

#include "nsIXPIDLPlugin.h"

class nsXPIDLPlugin : public nsIPlugin
{
public:
  NS_DECL_ISUPPORTS
//  NS_DECL_NSIXPIDLPLUGIN

  nsXPIDLPlugin();
  nsXPIDLPlugin( nsIXPIDLPlugin *plugin );
  virtual ~nsXPIDLPlugin();

  nsIXPIDLPlugin *plugin;

  // from nsIPlugin
  NS_IMETHODIMP CreatePluginInstance( nsISupports *aOuter,
                                      REFNSIID aIID,
                                      const char* aPluginMIMEType,
                                      void **aResult );
  NS_IMETHODIMP Initialize();
  NS_IMETHODIMP Shutdown();
};

#endif // _nsIXPIDLPlugin_included_
