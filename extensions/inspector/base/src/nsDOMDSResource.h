/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#ifndef __nsdomdsresource_h
#define __nsdomdsresource_h

#define NS_DOMDSRESOURCE_ID "InspectorDOMResource"

#include "nsIDOMDSResource.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFResource.h"

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

class nsDOMDSResource : public nsRDFResource,
                        public nsIDOMDSResource
{
  // nsDOMDSResource
public:
  nsDOMDSResource();
  virtual ~nsDOMDSResource();

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& iid, void **result);
  
private:
  nsCOMPtr<nsISupports> mObject;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  
  // nsIDOMDSResource
  NS_DECL_NSIDOMDSRESOURCE

};

#endif // __nsdomdsresource_h

