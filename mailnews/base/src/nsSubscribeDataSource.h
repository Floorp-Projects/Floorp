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
 * Seth Spitzer <sspitzer@netscape.com>
 */

#ifndef nsSubscribeDataSource_h__
#define nsSubscribeDataSource_h__

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFLiteral.h"
#include "nsCOMPtr.h"
#include "nsISubscribableServer.h"

/**
 * The subscribe data source.
 */
class nsSubscribeDataSource : public nsIRDFDataSource, public nsISubscribeDataSource
{

public:
	nsSubscribeDataSource();
	virtual ~nsSubscribeDataSource();

	nsresult Init();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFDATASOURCE
    NS_DECL_NSISUBSCRIBEDATASOURCE

private:
    nsCOMPtr <nsIRDFResource>      kNC_Child;
    nsCOMPtr <nsIRDFResource>      kNC_Name;    
    nsCOMPtr <nsIRDFResource>      kNC_LeafName;
    nsCOMPtr <nsIRDFResource>      kNC_Subscribed;
    nsCOMPtr <nsIRDFLiteral>       kTrueLiteral;
    nsCOMPtr <nsIRDFLiteral>       kFalseLiteral;

    nsCOMPtr <nsIRDFService>       mRDFService;
    nsCOMPtr <nsISupportsArray>    mObservers;

    nsresult GetChildren(nsISubscribableServer *server, const char *relativePath, nsISimpleEnumerator** aResult);
    nsresult GetServerAndRelativePathFromResource(nsIRDFResource *source, nsISubscribableServer **server, char **relativePath);

    static PRBool assertEnumFunc(nsISupports *aElement, void *aData);
    static PRBool unassertEnumFunc(nsISupports *aElement, void *aData);
    static PRBool changeEnumFunc(nsISupports *aElement, void *aData);
};

#endif /* nsSubscribedDataSource_h__ */
