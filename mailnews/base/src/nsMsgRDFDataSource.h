/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef __nsMsgRDFDataSource_h
#define __nsMsgRDFDataSource_h

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITransactionManager.h"
#include "nsIMsgWindowData.h"

class nsMsgRDFDataSource : public nsIRDFDataSource,
                           public nsIShutdownListener,
						   public nsIMsgWindowData
{
 public:
  nsMsgRDFDataSource();
  virtual ~nsMsgRDFDataSource();
  virtual nsresult Init();
  
  NS_DECL_ISUPPORTS
    
  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports* service);

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI);

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty,
                       nsIRDFNode *aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource **_retval);

  /* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty,
                        nsIRDFNode *aTarget,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource,
                       nsIRDFResource *aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource,
                        nsIRDFResource *aProperty,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* void Assert (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *aSource,
                    nsIRDFResource *aProperty,
                    nsIRDFNode *aTarget,
                    PRBool aTruthValue);

  /* void Unassert (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *aSource,
                      nsIRDFResource *aProperty,
                      nsIRDFNode *aTarget);

  /* void Change (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aOldTarget,
     in nsIRDFNode aNewTarget);
     */
  NS_IMETHOD Change(nsIRDFResource *aSource,
                    nsIRDFResource *aProperty,
                    nsIRDFNode *aOldTarget,
                    nsIRDFNode *aNewTarget);

  /* void Move (in nsIRDFResource aOldSource,
     in nsIRDFResource aNewSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget); */
  NS_IMETHOD Move(nsIRDFResource *aOldSource,
                  nsIRDFResource *aNewSource,
                  nsIRDFResource *aProperty,
                  nsIRDFNode *aTarget);

  /* boolean HasAssertion (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *aSource,
                          nsIRDFResource *aProperty,
                          nsIRDFNode *aTarget,
                          PRBool aTruthValue,
                          PRBool *_retval);

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver);

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver);

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode,
                         nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource,
                          nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval);

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *aSource,
                            nsIEnumerator **_retval);

  /* nsISimpleEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCmds(nsIRDFResource *aSource,
                            nsISimpleEnumerator **_retval);

  /* boolean IsCommandEnabled (in nsISupportsArray aSources,
     in nsIRDFResource aCommand,
     in nsISupportsArray aArguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray *aSources,
                              nsIRDFResource *aCommand,
                              nsISupportsArray *aArguments,
                              PRBool *_retval);

  /* void DoCommand (in nsISupportsArray aSources,
     in nsIRDFResource aCommand,
     in nsISupportsArray aArguments); */
  NS_IMETHOD DoCommand(nsISupportsArray *aSources,
                       nsIRDFResource *aCommand,
                       nsISupportsArray *aArguments);


  //nsIMsgWindowData interface
  NS_IMETHOD GetStatusFeedback(nsIMsgStatusFeedback * *aStatusFeedback);
  NS_IMETHOD SetStatusFeedback(nsIMsgStatusFeedback * aStatusFeedback);

  NS_IMETHOD GetTransactionManager(nsITransactionManager * *aTransactionManager);
  NS_IMETHOD SetTransactionManager(nsITransactionManager * aTransactionManager);

 protected:
	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool unassertEnumFunc(nsISupports *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert);
	nsresult GetTransactionManager(nsISupportsArray *sources, nsITransactionManager **aTransactionManager);

	nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;
	nsCOMPtr<nsITransactionManager> mTransactionManager;
 private:
  nsIRDFService *mRDFService;
  nsCOMPtr<nsISupportsArray> mObservers;

};

#endif
