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

#include "nsIRDFDataSource.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"

class nsMsgRDFDataSource : public nsIRDFDataSource,
                           public nsIShutdownListener
{
 public:
  nsMsgRDFDataSource();
  virtual ~nsMsgRDFDataSource();
  
  NS_DECL_ISUPPORTS
    
  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports* service);

  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI);

  /* nsIRDFResource GetSource (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *aProperty,
                       nsIRDFNode *aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource **_retval);

  /* nsIRDFAssertionCursor GetSources (in nsIRDFResource aProperty,
     in nsIRDFNode aTarget,
     in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *aProperty,
                        nsIRDFNode *aTarget,
                        PRBool aTruthValue,
                        nsIRDFAssertionCursor **_retval);

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *aSource,
                       nsIRDFResource *aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsIRDFAssertionCursor GetTargets (in nsIRDFResource aSource,
     in nsIRDFResource aProperty,
     in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *aSource,
                        nsIRDFResource *aProperty,
                        PRBool aTruthValue,
                        nsIRDFAssertionCursor **_retval);

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

  /* nsIRDFArcsInCursor ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode,
                         nsIRDFArcsInCursor **_retval);

  /* nsIRDFArcsOutCursor ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource,
                          nsIRDFArcsOutCursor **_retval);

  /* nsIRDFResourceCursor GetAllResources (); */
  NS_IMETHOD GetAllResources(nsIRDFResourceCursor **_retval);

  /* void Flush (); */
  NS_IMETHOD Flush();

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *aSource,
                            nsIEnumerator **_retval);

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


 protected:
  char *mURI;

  nsIRDFService *getRDFService();
  
 private:
  nsIRDFService *mRDFService;
  nsVoidArray *mObservers;

};

#endif
