/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#ifndef __inDOMRDFResource_h__
#define __inDOMRDFResource_h__

#include "inIDOMRDFResource.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFResource.h"

#define IN_DOMRDFRESOURCE_ID "InspectorDOMResource"

/////////////////////////////////////////////////////////////////////
// This object is registered with the datasource factory to
// be instantiated for all resources created with a uri that begins 
// with InspectorDOMResource://
//
// This is a multi-purpose object that is used by datasources as
// a temporary transport object for data.  When ::GetTargets is called,
// a list of these objects is created by the datasource.  Each object 
// holds a reference back to the real object it represents.  This
// object is then sent back to GetTarget, where we get the real 
// object and query it for info.
/////////////////////////////////////////////////////////////////////

class inDOMRDFResource : public nsRDFResource,
                         public inIDOMRDFResource
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_INIDOMRDFRESOURCE

  inDOMRDFResource();
  virtual ~inDOMRDFResource();

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& iid, void **result);
  
protected:
  nsCOMPtr<nsISupports> mObject;
};

#endif // __inDOMRDFResource_h__

