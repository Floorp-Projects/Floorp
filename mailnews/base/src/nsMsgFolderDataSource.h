/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"

#include "nsIFolderListener.h"
#include "nsMsgRDFDataSource.h"

#include "nsITransactionManager.h"

/**
 * The mail data source.
 */
class nsMsgFolderDataSource : public nsMsgRDFDataSource,
                              public nsIFolderListener
{
private:
	PRBool				mInitialized;

  // The cached service managers
  
  nsIRDFService* mRDFService;
  
public:
  
  NS_DECL_ISUPPORTS_INHERITED

  nsMsgFolderDataSource(void);
  virtual ~nsMsgFolderDataSource (void);
  virtual nsresult Init();

  // nsIRDFDataSource methods
  NS_IMETHOD GetURI(char* *uri);

  NS_IMETHOD GetSource(nsIRDFResource* property,
                       nsIRDFNode* target,
                       PRBool tv,
                       nsIRDFResource** source /* out */);

  NS_IMETHOD GetTarget(nsIRDFResource* source,
                       nsIRDFResource* property,
                       PRBool tv,
                       nsIRDFNode** target);

  NS_IMETHOD GetSources(nsIRDFResource* property,
                        nsIRDFNode* target,
                        PRBool tv,
                        nsISimpleEnumerator** sources);

  NS_IMETHOD GetTargets(nsIRDFResource* source,
                        nsIRDFResource* property,    
                        PRBool tv,
                        nsISimpleEnumerator** targets);

  NS_IMETHOD Assert(nsIRDFResource* source,
                    nsIRDFResource* property, 
                    nsIRDFNode* target,
                    PRBool tv);

  NS_IMETHOD Unassert(nsIRDFResource* source,
                      nsIRDFResource* property,
                      nsIRDFNode* target);

  NS_IMETHOD HasAssertion(nsIRDFResource* source,
                          nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          PRBool* hasAssertion);

  NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                         nsISimpleEnumerator** labels);

  NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                          nsISimpleEnumerator** labels); 

  NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult);

  NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                            nsIEnumerator/*<nsIRDFResource>*/** commands);
  NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                            nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

  NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                              nsIRDFResource*   aCommand,
                              nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                              PRBool* aResult);

  NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                       nsIRDFResource*   aCommand,
                       nsISupportsArray/*<nsIRDFResource>*/* aArguments);

  NS_IMETHOD OnItemAdded(nsIFolder *parentFolder, nsISupports *item);

  NS_IMETHOD OnItemRemoved(nsIFolder *parentFolder, nsISupports *item);

  NS_IMETHOD OnItemPropertyChanged(nsISupports *item, const char *property,
									const char *oldValue, const char *newValue);

  NS_IMETHOD OnItemPropertyFlagChanged(nsISupports *item, const char *property,
									   PRUint32 oldFlag, PRUint32 newFlag);

  // caching frequently used resources
protected:

	nsresult NotifyPropertyChanged(nsIRDFResource *resource, nsIRDFResource *propertyResource,
									const char *oldValue, const char *newValue);

	nsresult  GetSenderName(nsAutoString& sender, nsAutoString *senderUserName);

	nsresult createFolderNode(nsIMsgFolder *folder, nsIRDFResource* property,
                            nsIRDFNode **target);
	nsresult createFolderNameNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createFolderSpecialNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createTotalMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createBiffStateNode(nsIMsgFolder *folder, nsIRDFNode **target);

	nsresult createFolderChildNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createFolderMessageNode(nsIMsgFolder *folder, nsIRDFNode **target);

	nsresult createMessageNode(nsIMessage *message, nsIRDFResource *property,
                             nsIRDFNode **target);

  static nsresult getFolderArcLabelsOut(nsIMsgFolder *folder,
                                        nsISupportsArray **arcs);
  
  nsresult DoDeleteFromFolder(nsIMsgFolder *folder,
							  nsISupportsArray *arguments, nsITransactionManager *txnMgr);

  nsresult DoCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
						  nsITransactionManager *txnMgr, PRBool isMove);

  nsresult DoNewFolder(nsIMsgFolder *folder,
							  nsISupportsArray *arguments);

  nsresult DoFolderAssert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target);

  nsresult DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetBiffStateString(PRUint32 biffState, nsCAutoString & biffStateStr);

  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_MessageChild;
  static nsIRDFResource* kNC_Folder;
  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_Columns;
  static nsIRDFResource* kNC_MSGFolderRoot;
  static nsIRDFResource* kNC_SpecialFolder;
  static nsIRDFResource* kNC_TotalMessages;
  static nsIRDFResource* kNC_TotalUnreadMessages;
  static nsIRDFResource* kNC_Charset;
  static nsIRDFResource* kNC_BiffState;

  // commands
  static nsIRDFResource* kNC_Delete;
  static nsIRDFResource* kNC_NewFolder;
  static nsIRDFResource* kNC_GetNewMessages;
  static nsIRDFResource* kNC_Copy;
  static nsIRDFResource* kNC_Move;
  static nsIRDFResource* kNC_MarkAllMessagesRead;
  static nsIRDFResource* kNC_Compact;
  static nsIRDFResource* kNC_Rename;
  static nsIRDFResource* kNC_EmptyTrash;

};

PR_EXTERN(nsresult)
NS_NewMsgFolderDataSource(const nsIID& iid, void **result);
