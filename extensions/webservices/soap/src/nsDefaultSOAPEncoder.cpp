/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDefaultSOAPEncoder.h"
#include "nsSOAPUtils.h"
#include "nsISOAPParameter.h"
#include "nsXPIDLString.h"

nsDefaultSOAPEncoder::nsDefaultSOAPEncoder()
{
  NS_INIT_ISUPPORTS();
}

nsDefaultSOAPEncoder::~nsDefaultSOAPEncoder()
{
}

NS_IMPL_ISUPPORTS1(nsDefaultSOAPEncoder, nsISOAPEncoder)

/* nsIDOMElement parameterToElement (in nsISOAPParameter parameter, in string encodingStyle, in nsIDOMDocument document); */
NS_IMETHODIMP 
nsDefaultSOAPEncoder::ParameterToElement(nsISOAPParameter *parameter, 
					 const char *encodingStyle, 
					 nsIDOMDocument *document, 
					 nsIDOMElement **_retval)
{
  NS_ENSURE_ARG(parameter);
  NS_ENSURE_ARG(encodingStyle);
  NS_ENSURE_ARG(document);
  NS_ENSURE_ARG_POINTER(_retval);

  PRInt32 type;
  parameter->GetType(&type);

  nsXPIDLString name;
  parameter->GetName(getter_Copies(name));
  
  // If it's an unnamed parameter, we use the type names defined
  // by SOAP
  if (!name) {
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISOAPParameter elementToParameter (in nsIDOMElement element, in string encodingStyle); */
NS_IMETHODIMP 
nsDefaultSOAPEncoder::ElementToParameter(nsIDOMElement *element, 
					 const char *encodingStyle, 
					 nsISOAPParameter **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
