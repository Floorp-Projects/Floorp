/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __nsMsgAccountManagerDS_h
#define __nsMsgAccountManagerDS_h

#include "nscore.h"
#include "nsError.h"
#include "nsIID.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"

#include "nsMsgRDFDataSource.h"
#include "nsIMsgAccountManager.h"
#include "nsIIncomingServerListener.h"
#include "nsWeakPtr.h"

/* {3f989ca4-f77a-11d2-969d-006008948010} */
#define NS_MSGACCOUNTMANAGERDATASOURCE_CID \
  {0x3f989ca4, 0xf77a, 0x11d2, \
    {0x96, 0x9d, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}

class nsMsgAccountManagerDataSource : public nsMsgRDFDataSource,
                                      public nsIFolderListener,
                                      public nsIIncomingServerListener
{

public:
    
  nsMsgAccountManagerDataSource();
  virtual ~nsMsgAccountManagerDataSource();
  virtual nsresult Init();

  virtual void Cleanup();
  // service manager shutdown method

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIFOLDERLISTENER
    NS_DECL_NSIINCOMINGSERVERLISTENER
  // RDF datasource methods
  
  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *source,
                       nsIRDFResource *property,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *source,
                        nsIRDFResource *property,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);
  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsISimpleEnumerator **_retval);

  NS_IMETHOD HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty,
                          nsIRDFNode *aTarget, PRBool aTruthValue,
                          PRBool *_retval);
    
protected:

  nsresult HasAssertionServer(nsIMsgIncomingServer *aServer,
                              nsIRDFResource *aProperty,
                              nsIRDFNode *aTarget,
                              PRBool aTruthValue, PRBool *_retval);

  nsresult HasAssertionAccountRoot(nsIRDFResource *aProperty,
                                   nsIRDFNode *aTarget,
                                   PRBool aTruthValue, PRBool *_retval);
  
  PRBool isDefaultServer(nsIMsgIncomingServer *aServer);
  PRBool supportsFilters(nsIMsgIncomingServer *aServer);
  
  static PRBool isContainment(nsIRDFResource *aProperty);
  nsresult getServerForFolderNode(nsIRDFNode *aResource,
                                  nsIMsgIncomingServer **aResult);
  nsresult getServerForObject(nsISupports *aObject,
                              nsIMsgIncomingServer **aResult);
  
  nsresult createRootResources(nsIRDFResource *aProperty,
                               nsISupportsArray* aNodeArray);
  nsresult createSettingsResources(nsIRDFResource *aSource,
                                   nsISupportsArray *aNodeArray);

  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_FolderTreeName;
  static nsIRDFResource* kNC_NameSort;
  static nsIRDFResource* kNC_FolderTreeNameSort;
  static nsIRDFResource* kNC_PageTag;
  static nsIRDFResource* kNC_IsDefaultServer;
  static nsIRDFResource* kNC_SupportsFilters;
  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_AccountRoot;
  
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;
  static nsIRDFResource* kNC_Settings;

  static nsIRDFResource* kNC_PageTitleMain;
  static nsIRDFResource* kNC_PageTitleServer;
  static nsIRDFResource* kNC_PageTitleCopies;
  static nsIRDFResource* kNC_PageTitleAdvanced;
  static nsIRDFResource* kNC_PageTitleSMTP;

  static nsIRDFLiteral* kTrueLiteral;

  static nsIAtom* kDefaultServerAtom;

  static nsrefcnt gAccountManagerResourceRefCnt;

  static nsresult getAccountArcs(nsISupportsArray **aResult);
  static nsresult getAccountRootArcs(nsISupportsArray **aResult);
  
private:
  // enumeration function to convert each server (element)
  // to an nsIRDFResource and append it to the array (in data)
  static PRBool createServerResources(nsISupports *element, void *data);

  // search for an account by key
  static PRBool findServerByKey(nsISupports *aElement, void *aData);

  nsresult serverHasIdentities(nsIMsgIncomingServer *aServer, PRBool *aResult);
  nsresult getStringBundle();

  static nsCOMPtr<nsISupportsArray> mAccountArcsOut;
  static nsCOMPtr<nsISupportsArray> mAccountRootArcsOut;
  nsWeakPtr mAccountManager;

  nsCOMPtr<nsIStringBundle> mStringBundle;

};



#endif
