/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

  nsresult GetSenderName(nsAutoString& sender, nsAutoString *senderUserName);

  nsresult createFolderNode(nsIMsgFolder *folder, nsIRDFResource* property,
                            nsIRDFNode **target);
  nsresult createFolderNameNode(nsIMsgFolder *folder, nsIRDFNode **target, PRBool sort);
  nsresult createFolderOpenNode(nsIMsgFolder *folder,nsIRDFNode **target);
  nsresult createFolderTreeNameNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderTreeSimpleNameNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderSpecialNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderServerTypeNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createServerIsDeferredNode(nsIMsgFolder* folder,
                                      nsIRDFNode **target);
  nsresult createFolderRedirectorTypeNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanCreateFoldersOnServerNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanFileMessagesOnServerNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderIsServerNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderIsSecureNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanSubscribeNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderSupportsOfflineNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanFileMessagesNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanCreateSubfoldersNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanRenameNode(nsIMsgFolder *folder,
                                      nsIRDFNode **target);
  nsresult createFolderCanCompactNode(nsIMsgFolder *folder,
                                     nsIRDFNode **target);
  nsresult createTotalMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderSizeNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createBiffStateNodeFromFolder(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createBiffStateNodeFromFlag(PRUint32 flag, nsIRDFNode **target);
  nsresult createHasUnreadMessagesNode(nsIMsgFolder *folder, PRBool aIncludeSubfolders, nsIRDFNode **target);
  nsresult createNewMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderNoSelectNode(nsIMsgFolder *folder,
                                    nsIRDFNode **target);
  nsresult createFolderImapSharedNode(nsIMsgFolder *folder,
                                    nsIRDFNode **target);
  nsresult createFolderSynchronizeNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createFolderSyncDisabledNode(nsIMsgFolder *folder, nsIRDFNode **target);
  nsresult createCanSearchMessages(nsIMsgFolder *folder,
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
  nsresult DoFolderUnassert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target);

  nsresult DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetBiffStateString(PRUint32 biffState, nsCAutoString & biffStateStr);

	nsresult CreateUnreadMessagesNameString(PRInt32 unreadMessages, nsAutoString &nameString);
	nsresult CreateArcsOutEnumerator();

  nsresult OnItemAddedOrRemoved(nsIRDFResource *parentItem, nsISupports *item, PRBool added);

  nsresult OnUnreadMessagePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue);
  nsresult OnTotalMessagePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue);
  nsresult OnFolderSizePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue);
  nsresult NotifyFolderTreeNameChanged(nsIMsgFolder *folder, nsIRDFResource *folderResource, PRInt32 aUnreadMessages);
  nsresult NotifyFolderTreeSimpleNameChanged(nsIMsgFolder *folder, nsIRDFResource *folderResource);
  nsresult NotifyFolderNameChanged(nsIMsgFolder *folder, nsIRDFResource *folderResource);
  nsresult NotifyAncestors(nsIMsgFolder *aFolder, nsIRDFResource *aPropertyResource, nsIRDFNode *aNode);
  nsresult GetNumMessagesNode(PRInt32 numMessages, nsIRDFNode **node);
  nsresult GetFolderSizeNode(PRInt32 folderSize, nsIRDFNode **node);
  nsresult CreateLiterals(nsIRDFService *rdf);

  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Folder;
  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_Open;
  static nsIRDFResource* kNC_FolderTreeName;
  static nsIRDFResource* kNC_FolderTreeSimpleName;
  static nsIRDFResource* kNC_NameSort;
  static nsIRDFResource* kNC_FolderTreeNameSort;
  static nsIRDFResource* kNC_Columns;
  static nsIRDFResource* kNC_MSGFolderRoot;
  static nsIRDFResource* kNC_SpecialFolder;
  static nsIRDFResource* kNC_ServerType;
  static nsIRDFResource* kNC_IsDeferred;
  static nsIRDFResource* kNC_RedirectorType;
  static nsIRDFResource* kNC_CanCreateFoldersOnServer;
  static nsIRDFResource* kNC_CanFileMessagesOnServer;
  static nsIRDFResource* kNC_IsServer;
  static nsIRDFResource* kNC_IsSecure;
  static nsIRDFResource* kNC_CanSubscribe;
  static nsIRDFResource* kNC_SupportsOffline;
  static nsIRDFResource* kNC_CanFileMessages;
  static nsIRDFResource* kNC_CanCreateSubfolders;
  static nsIRDFResource* kNC_CanRename;
  static nsIRDFResource* kNC_CanCompact;
  static nsIRDFResource* kNC_TotalMessages;
  static nsIRDFResource* kNC_TotalUnreadMessages;
  static nsIRDFResource* kNC_FolderSize;
  static nsIRDFResource* kNC_Charset;
  static nsIRDFResource* kNC_BiffState;
  static nsIRDFResource* kNC_HasUnreadMessages;
  static nsIRDFResource* kNC_NewMessages;
  static nsIRDFResource* kNC_SubfoldersHaveUnreadMessages;
  static nsIRDFResource* kNC_NoSelect;
  static nsIRDFResource* kNC_ImapShared;
  static nsIRDFResource* kNC_Synchronize;
  static nsIRDFResource* kNC_SyncDisabled;
  static nsIRDFResource* kNC_CanSearchMessages;
  

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
  static nsIRDFResource* kNC_CompactAll;
  static nsIRDFResource* kNC_Rename;
  static nsIRDFResource* kNC_EmptyTrash;
  static nsIRDFResource* kNC_DownloadFlagged;
  //Cached literals
  nsCOMPtr<nsIRDFNode> kTrueLiteral;
  nsCOMPtr<nsIRDFNode> kFalseLiteral;

  // property atoms
  static nsIAtom* kTotalMessagesAtom;
  static nsIAtom* kTotalUnreadMessagesAtom;
  static nsIAtom* kFolderSizeAtom;
  static nsIAtom* kBiffStateAtom;
  static nsIAtom* kNewMessagesAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kSynchronizeAtom;
  static nsIAtom* kOpenAtom;
  static nsIAtom* kIsDeferredAtom;
  static nsrefcnt gFolderResourceRefCnt;
  static nsIAtom* kCanFileMessagesAtom;
  
  nsCOMPtr<nsISupportsArray> kFolderArcsOutArray;

};


