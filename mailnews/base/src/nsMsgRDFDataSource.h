/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __nsMsgRDFDataSource_h
#define __nsMsgRDFDataSource_h

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIObserver.h"
#include "nsITransactionManager.h"
#include "nsIMsgWindow.h"
#include "nsIMsgRDFDataSource.h"
#include "nsWeakReference.h"

class nsMsgRDFDataSource : public nsIRDFDataSource,
                           public nsIObserver,
                           public nsSupportsWeakReference,
  						   public nsIMsgRDFDataSource
{
 public:
  nsMsgRDFDataSource();
  virtual ~nsMsgRDFDataSource();
  virtual nsresult Init();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGRDFDATASOURCE
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIOBSERVER

  // called to reset the datasource to an empty state
  // if you need to release yourself as an observer/listener, do it here
  virtual void Cleanup();

 protected:
	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool unassertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool changeEnumFunc(nsISupports *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert, PRBool change);
	nsresult NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  const char *newValue);
	nsresult NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  nsIRDFNode *newNode);
	nsresult GetTransactionManager(nsISupportsArray *sources, nsITransactionManager **aTransactionManager);

	nsCOMPtr<nsIMsgWindow> mWindow;

	PRBool m_shuttingDown;
    PRBool mInitialized;

 private:
  nsCOMPtr<nsIRDFService> mRDFService;
  nsCOMPtr<nsISupportsArray> mObservers;

};

#endif
