/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIMsgProtocolInfo.h"
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
  NS_IMETHOD HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result);
    
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
  PRBool canGetMessages(nsIMsgIncomingServer *aServer);
  
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
  static nsIRDFResource* kNC_FolderTreeSimpleName;
  static nsIRDFResource* kNC_NameSort;
  static nsIRDFResource* kNC_FolderTreeNameSort;
  static nsIRDFResource* kNC_PageTag;
  static nsIRDFResource* kNC_IsDefaultServer;
  static nsIRDFResource* kNC_SupportsFilters;
  static nsIRDFResource* kNC_CanGetMessages;
  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_AccountRoot;
  
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;
  static nsIRDFResource* kNC_Settings;

  static nsIRDFResource* kNC_PageTitleMain;
  static nsIRDFResource* kNC_PageTitleServer;
  static nsIRDFResource* kNC_PageTitleCopies;
  static nsIRDFResource* kNC_PageTitleOfflineAndDiskSpace;
  static nsIRDFResource* kNC_PageTitleDiskSpace;
  static nsIRDFResource* kNC_PageTitleAddressing;
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
