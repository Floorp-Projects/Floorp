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

#ifndef _nsXPIDLPluginInstance_included_
#define _nsXPIDLPluginInstance_included_

#include "nsIPluginInstance.h"

#include "nsIXPIDLPluginInstance.h"
#include "nsIPluginInstancePeer.h"
#include "nsXPIDLPluginInstancePeer.h"
#include "nsIPluginStreamListener.h"
#include "nsXPIDLPluginStreamListener.h"

class nsXPIDLPluginInstance : public nsIPluginInstance
{
public:
  NS_DECL_ISUPPORTS
//  NS_DECL_NSIXPIDLPLUGININSTANCE

  nsXPIDLPluginInstance();
  nsXPIDLPluginInstance( nsIXPIDLPluginInstance *pluginInstance );
  virtual ~nsXPIDLPluginInstance();

  nsIXPIDLPluginInstance *pluginInstance;

  // from nsIPluginInstance
  NS_IMETHODIMP Destroy();
  NS_IMETHODIMP Initialize( nsIPluginInstancePeer *peer );
  NS_IMETHODIMP NewStream( nsIPluginStreamListener **_retval );
  NS_IMETHODIMP Start();
  NS_IMETHODIMP Stop();
};

#endif // _nsXPIDLPluginInstance_included_
