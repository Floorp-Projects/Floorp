/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef FORWARDPROXYDATASOURCE___H___
#define FORWARDPROXYDATASOURCE___H___

#include "nsCOMArray.h"
#include "nsIRDFService.h"
#include "nsIRDFInferDataSource.h"
#include "nsIRDFDataSource.h"

class nsForwardProxyDataSource : public nsIRDFInferDataSource,
                                 public nsIRDFObserver
{
public:
    nsForwardProxyDataSource();

    nsresult Init(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_DECL_NSIRDFDATASOURCE

    // nsIRDFObserver interface
    NS_DECL_NSIRDFOBSERVER

    // nsIRDFInferDataSource interface
    NS_DECL_NSIRDFINFERDATASOURCE

protected:
    nsCOMPtr<nsIRDFDataSource> mDS;
    nsCOMPtr<nsIRDFResource> mProxyProperty;

    nsCOMArray<nsIRDFObserver> mObservers;

    PRInt32 mUpdateBatchNest;

    nsresult GetProxyResource (nsIRDFResource *aResource, nsIRDFResource **aResult);
    nsresult GetRealSource (nsIRDFResource *aResource, nsIRDFResource **aResult);
    nsresult GetProxyResourcesArray (nsISupportsArray* aSources, nsISupportsArray** aSourcesAndProxies);

    virtual ~nsForwardProxyDataSource();
};

#endif /* FORWARDPROXYDATASOURCE___H___ */
