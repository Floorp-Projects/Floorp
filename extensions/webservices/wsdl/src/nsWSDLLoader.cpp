/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsWSDLPrivate.h"
#include "nsIXMLHttpRequest.h"

nsWSDLLoader::nsWSDLLoader()
{
  NS_INIT_ISUPPORTS();
}

nsWSDLLoader::~nsWSDLLoader()
{
}

NS_IMPL_ISUPPORTS1(nsWSDLLoader, nsIWSDLLoader)

/* nsIWSDLService loadService (in nsIURI wsdlURI); */
NS_IMETHODIMP 
nsWSDLLoader::LoadService(nsIURI *wsdlURI, 
			  nsIWSDLService **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISupports createServiceProxy (in nsIWSDLService service, in wstring nameSpace); */
NS_IMETHODIMP 
nsWSDLLoader::CreateServiceProxy(nsIWSDLService *service, 
                                 const PRUnichar *nameSpace, 
                                 nsISupports **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

