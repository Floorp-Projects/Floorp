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

//This file holds some useful utility functions and declarations used by our datasources.

#include "rdf.h"
#include "nsIRDFResource.h"
#include "nsIRDFNode.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsString.h"

// this is used for notification of observers using nsVoidArray
typedef struct _nsMsgRDFNotification {
  nsIRDFDataSource *datasource;
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *newObject;
  nsIRDFNode *oldObject;
} nsMsgRDFNotification;

//Some property declarations

#define NC_RDF_CHILD				NC_NAMESPACE_URI "child"
#define NC_RDF_NAME					NC_NAMESPACE_URI "Name"
#define NC_RDF_OPEN					NC_NAMESPACE_URI "open"
#define NC_RDF_FOLDERTREENAME		NC_NAMESPACE_URI "FolderTreeName"
#define NC_RDF_FOLDERTREESIMPLENAME		NC_NAMESPACE_URI "FolderTreeSimpleName"
#define NC_RDF_FOLDER				NC_NAMESPACE_URI "Folder"
#define NC_RDF_SPECIALFOLDER		NC_NAMESPACE_URI "SpecialFolder"
#define NC_RDF_SERVERTYPE			NC_NAMESPACE_URI "ServerType"
#define NC_RDF_REDIRECTORTYPE			NC_NAMESPACE_URI "RedirectorType"
#define NC_RDF_CANCREATEFOLDERSONSERVER	NC_NAMESPACE_URI "CanCreateFoldersOnServer"
#define NC_RDF_CANFILEMESSAGESONSERVER	NC_NAMESPACE_URI "CanFileMessagesOnServer"
#define NC_RDF_ISSERVER				NC_NAMESPACE_URI "IsServer"
#define NC_RDF_ISSECURE             NC_NAMESPACE_URI "IsSecure"
#define NC_RDF_CANSUBSCRIBE			NC_NAMESPACE_URI "CanSubscribe"
#define NC_RDF_SUPPORTSOFFLINE                  NC_NAMESPACE_URI "SupportsOffline"
#define NC_RDF_CANFILEMESSAGES			NC_NAMESPACE_URI "CanFileMessages"
#define NC_RDF_CANCREATESUBFOLDERS			NC_NAMESPACE_URI "CanCreateSubfolders"
#define NC_RDF_CANRENAME			NC_NAMESPACE_URI "CanRename"
#define NC_RDF_CANCOMPACT			NC_NAMESPACE_URI "CanCompact"
#define NC_RDF_TOTALMESSAGES		NC_NAMESPACE_URI "TotalMessages"
#define NC_RDF_TOTALUNREADMESSAGES	NC_NAMESPACE_URI "TotalUnreadMessages"
#define NC_RDF_FOLDERSIZE               NC_NAMESPACE_URI "FolderSize"
#define NC_RDF_CHARSET				NC_NAMESPACE_URI "Charset"
#define NC_RDF_BIFFSTATE			NC_NAMESPACE_URI "BiffState"
#define NC_RDF_HASUNREADMESSAGES	NC_NAMESPACE_URI "HasUnreadMessages"
#define NC_RDF_SUBFOLDERSHAVEUNREADMESSAGES NC_NAMESPACE_URI "SubfoldersHaveUnreadMessages"
#define NC_RDF_NOSELECT       NC_NAMESPACE_URI "NoSelect"
#define NC_RDF_VIRTUALFOLDER   NC_NAMESPACE_URI "Virtual"
#define NC_RDF_IMAPSHARED     NC_NAMESPACE_URI "ImapShared"
#define NC_RDF_NEWMESSAGES			NC_NAMESPACE_URI "NewMessages"
#define NC_RDF_SYNCHRONIZE          NC_NAMESPACE_URI "Synchronize"
#define NC_RDF_SYNCDISABLED         NC_NAMESPACE_URI "SyncDisabled"
#define NC_RDF_KEY                      NC_NAMESPACE_URI "Key"
#define NC_RDF_CANSEARCHMESSAGES			NC_NAMESPACE_URI "CanSearchMessages"
#define NC_RDF_ISDEFERRED NC_NAMESPACE_URI "IsDeferred"

//Sort Properties
#define NC_RDF_SUBJECT_COLLATION_SORT	NC_NAMESPACE_URI "Subject?collation=true"
#define NC_RDF_SENDER_COLLATION_SORT	NC_NAMESPACE_URI "Sender?collation=true"
#define NC_RDF_RECIPIENT_COLLATION_SORT	NC_NAMESPACE_URI "Recipient?collation=true"
#define NC_RDF_ORDERRECEIVED_SORT		NC_NAMESPACE_URI "OrderReceived?sort=true"
#define NC_RDF_PRIORITY_SORT			NC_NAMESPACE_URI "Priority?sort=true"
#define NC_RDF_DATE_SORT                NC_NAMESPACE_URI "Date?sort=true"
#define NC_RDF_SIZE_SORT				NC_NAMESPACE_URI "Size?sort=true"
#define NC_RDF_ISUNREAD_SORT			NC_NAMESPACE_URI "IsUnread?sort=true"
#define NC_RDF_FLAGGED_SORT				NC_NAMESPACE_URI "Flagged?sort=true"

#define NC_RDF_NAME_SORT				NC_NAMESPACE_URI "Name?sort=true"
#define NC_RDF_FOLDERTREENAME_SORT		NC_NAMESPACE_URI "FolderTreeName?sort=true"

//Folder Commands
#define NC_RDF_DELETE				NC_NAMESPACE_URI "Delete"
#define NC_RDF_REALLY_DELETE		NC_NAMESPACE_URI "ReallyDelete"
#define NC_RDF_NEWFOLDER			NC_NAMESPACE_URI "NewFolder"
#define NC_RDF_GETNEWMESSAGES		NC_NAMESPACE_URI "GetNewMessages"
#define NC_RDF_COPY					NC_NAMESPACE_URI "Copy"
#define NC_RDF_MOVE					NC_NAMESPACE_URI "Move"
#define NC_RDF_COPYFOLDER			NC_NAMESPACE_URI "CopyFolder"
#define NC_RDF_MOVEFOLDER			NC_NAMESPACE_URI "MoveFolder"
#define NC_RDF_MARKALLMESSAGESREAD  NC_NAMESPACE_URI "MarkAllMessagesRead"
#define NC_RDF_COMPACT				NC_NAMESPACE_URI "Compact"
#define NC_RDF_COMPACTALL			NC_NAMESPACE_URI "CompactAll"
#define NC_RDF_RENAME				NC_NAMESPACE_URI "Rename"
#define NC_RDF_EMPTYTRASH   NC_NAMESPACE_URI "EmptyTrash"
#define NC_RDF_DOWNLOADFLAGGED NC_NAMESPACE_URI "DownloadFlaggedMessages"
#define NC_RDF_DOWNLOADSELECTED NC_NAMESPACE_URI "DownloadSelectedMessages"


nsresult createNode(const PRUnichar *str, nsIRDFNode **, nsIRDFService *rdfService);

//Given an PRInt32 creates an nsIRDFNode that is really an int literal.
nsresult createIntNode(PRInt32 value, nsIRDFNode **node, nsIRDFService *rdfService);

//Given an nsIRDFBlob creates an nsIRDFNode that is really an blob literal.
nsresult createBlobNode(PRUint8 *value, PRUint32 &length,  nsIRDFNode **node, nsIRDFService *rdfService);

//s Assertion for a datasource that will just call GetTarget on property.  When all of our 
//datasource derive from our datasource baseclass, this should be moved there and the first
//parameter will no longer be needed.
nsresult GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* folderResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion);


