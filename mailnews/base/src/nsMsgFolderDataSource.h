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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
public:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFOLDERLISTENER

  nsMsgFolderDataSource(void);
  virtual ~nsMsgFolderDataSource (void);
  virtual nsresult Init();
  virtual void Cleanup();

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

  NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result);

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
protected:

	nsresult  GetSenderName(nsAutoString& sender, nsAutoString *senderUserName);

	nsresult createFolderNode(nsIMsgFolder *folder, nsIRDFResource* property,
                            nsIRDFNode **target);
	nsresult createFolderNameNode(nsIMsgFolder *folder, nsIRDFNode **target, PRBool sort);
	nsresult createFolderTreeNameNode(nsIMsgFolder *folder, nsIRDFNode **target, PRBool sort);
	nsresult createFolderSpecialNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderServerTypeNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderIsServerNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderIsSecureNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanSubscribeNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanFileMessagesNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanCreateSubfoldersNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanRenameNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
	nsresult createTotalMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createBiffStateNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createHasUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createNewMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createSubfoldersHaveUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderNoSelectNode(nsIMsgFolder *folder,
                                    nsIRDFNode **target);

	nsresult createFolderChildNode(nsIMsgFolder *folder, nsIRDFNode **target);

  nsresult getFolderArcLabelsOut(nsISupportsArray **arcs);
  
  nsresult DoDeleteFromFolder(nsIMsgFolder *folder,
							  nsISupportsArray *arguments, nsIMsgWindow *msgWindow, PRBool reallyDelete);

  nsresult DoCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
						  nsIMsgWindow *msgWindow, PRBool isMove);

  nsresult DoFolderCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
						  nsIMsgWindow *msgWindow, PRBool isMoveFolder);

  nsresult DoNewFolder(nsIMsgFolder *folder,
							  nsISupportsArray *arguments);

  nsresult DoFolderAssert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target);

  nsresult DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetBiffStateString(PRUint32 biffState, nsCAutoString & biffStateStr);
	nsresult GetNewMessagesString(PRBool newMessages, nsCAutoString & newMessagesStr);

	nsresult CreateNameSortString(nsIMsgFolder *folder, nsAutoString &name);
	nsresult CreateUnreadMessagesNameString(PRInt32 unreadMessages, nsAutoString &nameString);
	nsresult GetFolderSortOrder(nsIMsgFolder *folder, PRInt32* order);

	nsresult CreateArcsOutEnumerator();

	nsresult OnItemAddedOrRemoved(nsISupports *parentItem, nsISupports *item,
		const char* viewString, PRBool added);

	nsresult OnUnreadMessagePropertyChanged(nsIMsgFolder *folder, PRInt32 oldValue, PRInt32 newValue);
	nsresult OnTotalMessagePropertyChanged(nsIMsgFolder *folder, PRInt32 oldValue, PRInt32 newValue);
  nsresult NotifyFolderTreeNameChanged(nsIMsgFolder *folder, PRInt32 aUnreadMessages);

	nsresult GetNumMessagesNode(PRInt32 numMessages, nsIRDFNode **node);

	nsresult CreateLiterals(nsIRDFService *rdf);

  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_MessageChild;
  static nsIRDFResource* kNC_Folder;
  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_FolderTreeName;
  static nsIRDFResource* kNC_NameSort;
  static nsIRDFResource* kNC_FolderTreeNameSort;
  static nsIRDFResource* kNC_Columns;
  static nsIRDFResource* kNC_MSGFolderRoot;
  static nsIRDFResource* kNC_SpecialFolder;
  static nsIRDFResource* kNC_ServerType;
  static nsIRDFResource* kNC_IsServer;
  static nsIRDFResource* kNC_IsSecure;
  static nsIRDFResource* kNC_CanSubscribe;
  static nsIRDFResource* kNC_CanFileMessages;
  static nsIRDFResource* kNC_CanCreateSubfolders;
  static nsIRDFResource* kNC_CanRename;
  static nsIRDFResource* kNC_TotalMessages;
  static nsIRDFResource* kNC_TotalUnreadMessages;
  static nsIRDFResource* kNC_Charset;
  static nsIRDFResource* kNC_BiffState;
  static nsIRDFResource* kNC_HasUnreadMessages;
  static nsIRDFResource* kNC_NewMessages;
  static nsIRDFResource* kNC_SubfoldersHaveUnreadMessages;
  static nsIRDFResource* kNC_NoSelect;

  // commands
  static nsIRDFResource* kNC_Delete;
  static nsIRDFResource* kNC_ReallyDelete;
  static nsIRDFResource* kNC_NewFolder;
  static nsIRDFResource* kNC_GetNewMessages;
  static nsIRDFResource* kNC_Copy;
  static nsIRDFResource* kNC_Move;
  static nsIRDFResource* kNC_CopyFolder;
  static nsIRDFResource* kNC_MoveFolder;
  static nsIRDFResource* kNC_MarkAllMessagesRead;
  static nsIRDFResource* kNC_Compact;
  static nsIRDFResource* kNC_Rename;
  static nsIRDFResource* kNC_EmptyTrash;
  static nsIRDFResource* kNC_DownloadFlagged;
  //Cached literals
  nsCOMPtr<nsIRDFNode> kTrueLiteral;
  nsCOMPtr<nsIRDFNode> kFalseLiteral;

  // property atoms
  static nsIAtom* kTotalMessagesAtom;
  static nsIAtom* kTotalUnreadMessagesAtom;
  static nsIAtom* kBiffStateAtom;
  static nsIAtom* kNewMessagesAtom;
  static nsIAtom* kNameAtom;
  
  static nsrefcnt gFolderResourceRefCnt;

	nsCOMPtr<nsISupportsArray> kFolderArcsOutArray;

};


