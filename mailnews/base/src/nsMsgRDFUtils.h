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

//This file holds some useful utility functions and declarations used by our datasources.

#include "rdf.h"
#include "nsIRDFResource.h"
#include "nsIRDFNode.h"
#include "nsIRDFDataSource.h"
#include "nsString.h"

// this is used for notification of observers using nsVoidArray
typedef struct _nsMsgRDFNotification {
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsMsgRDFNotification;

//Some property declarations

#define NC_RDF_SUBJECT				NC_NAMESPACE_URI "Subject"
#define NC_RDF_SENDER				NC_NAMESPACE_URI "Sender"
#define NC_RDF_DATE					NC_NAMESPACE_URI "Date"
#define NC_RDF_STATUS				NC_NAMESPACE_URI "Status"
#define NC_RDF_FLAGGED				NC_NAMESPACE_URI "Flagged"

#define NC_RDF_CHILD				NC_NAMESPACE_URI "child"
#define NC_RDF_MESSAGECHILD			NC_NAMESPACE_URI "MessageChild"
#define NC_RDF_NAME					NC_NAMESPACE_URI "Name"
#define NC_RDF_FOLDER				NC_NAMESPACE_URI "Folder"
#define NC_RDF_SPECIALFOLDER		NC_NAMESPACE_URI "SpecialFolder"
#define NC_RDF_SERVERTYPE   NC_NAMESPACE_URI "ServerType"
#define NC_RDF_ISSERVER   NC_NAMESPACE_URI "IsServer"
#define NC_RDF_TOTALMESSAGES		NC_NAMESPACE_URI "TotalMessages"
#define NC_RDF_TOTALUNREADMESSAGES	NC_NAMESPACE_URI "TotalUnreadMessages"
#define NC_RDF_CHARSET				NC_NAMESPACE_URI "Charset"
#define NC_RDF_BIFFSTATE			NC_NAMESPACE_URI "BiffState"

//Folder Commands
#define NC_RDF_DELETE				NC_NAMESPACE_URI "Delete"
#define NC_RDF_NEWFOLDER			NC_NAMESPACE_URI "NewFolder"
#define NC_RDF_GETNEWMESSAGES		NC_NAMESPACE_URI "GetNewMessages"
#define NC_RDF_COPY					NC_NAMESPACE_URI "Copy"
#define NC_RDF_MOVE					NC_NAMESPACE_URI "Move"
#define NC_RDF_MARKALLMESSAGESREAD  NC_NAMESPACE_URI "MarkAllMessagesRead"
#define NC_RDF_COMPACT				NC_NAMESPACE_URI "Compact"
#define NC_RDF_RENAME				NC_NAMESPACE_URI "Rename"
#define NC_RDF_EMPTYTRASH   NC_NAMESPACE_URI "EmptyTrash"

//Message Commands
#define NC_RDF_MARKREAD				NC_NAMESPACE_URI "MarkRead"
#define NC_RDF_MARKUNREAD			NC_NAMESPACE_URI "MarkUnread"
#define NC_RDF_TOGGLEREAD			NC_NAMESPACE_URI "ToggleRead"
#define NC_RDF_MARKFLAGGED			NC_NAMESPACE_URI "MarkFlagged"
#define NC_RDF_MARKUNFLAGGED		NC_NAMESPACE_URI "MarkUnflagged"

//Returns PR_TRUE if r1 is equal to r2 and r2 is the sort property.
PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isSort);

//Returns PR_TRUE if r1 is equal to r2 and r2 is the collation property.
PRBool
peqCollationSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isCollation);

//Given an nsString, create an nsIRDFNode
nsresult createNode(nsString& str, nsIRDFNode **node);
nsresult createNode(const char*, nsIRDFNode **);

//Given a PRUint32, create an nsiIRDFNode.
nsresult createNode(PRUint32 value, nsIRDFNode **node);

//Given a PRTime create an nsIRDFNode that is really a date literal.
nsresult createDateNode(PRTime time, nsIRDFNode **node);

//Has Assertion for a datasource that will just call GetTarget on property.  When all of our 
//datasource derive from our datasource baseclass, this should be moved there and the first
//parameter will no longer be needed.
nsresult GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* folderResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion);


