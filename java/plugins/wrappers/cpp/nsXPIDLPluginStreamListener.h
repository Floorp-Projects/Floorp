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

#ifndef _nsXPIDLPluginStreamListener_included_
#define _nsXPIDLPluginStreamListener_included_

#include "nsIPluginStreamListener.h"

#include "nsIXPIDLPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"
#include "nsXPIDLPluginStreamInfo.h"
#include "nsIInputStream.h"
#include "nsXPIDLInputStream.h"

class nsXPIDLPluginStreamListener : public nsIPluginStreamListener
{
public:
  NS_DECL_ISUPPORTS
//  NS_DECL_NSIXPIDLPLUGINSTREAMLISTENER

  nsXPIDLPluginStreamListener();
  nsXPIDLPluginStreamListener( nsIXPIDLPluginStreamListener * pluginStreamListener );
  virtual ~nsXPIDLPluginStreamListener();

  nsIXPIDLPluginStreamListener * pluginStreamListener;

  // from nsIPluginStreamListener
  NS_IMETHODIMP GetStreamType( nsPluginStreamType *result );
  NS_IMETHODIMP OnDataAvailable( nsIPluginStreamInfo *streamInfo,
                                 nsIInputStream *input,
                                 PRUint32 length );
  NS_IMETHODIMP OnFileAvailable( nsIPluginStreamInfo *streamInfo,
                                 const char *fileName );
  NS_IMETHODIMP OnStartBinding( nsIPluginStreamInfo *streamInfo );
  NS_IMETHODIMP OnStopBinding( nsIPluginStreamInfo *streamInfo, nsresult status );
};

#endif // _nsXPIDLPluginStreamListener_included_
