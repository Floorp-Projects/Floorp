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

#define NC_RDF_SUBJECT				"http://home.netscape.com/NC-rdf#Subject"
#define NC_RDF_SENDER				"http://home.netscape.com/NC-rdf#Sender"
#define NC_RDF_DATE					"http://home.netscape.com/NC-rdf#Date"
#define NC_RDF_STATUS				"http://home.netscape.com/NC-rdf#Status"

#define NC_RDF_CHILD				"http://home.netscape.com/NC-rdf#child"
#define NC_RDF_MESSAGECHILD			"http://home.netscape.com/NC-rdf#MessageChild"
#define NC_RDF_NAME					"http://home.netscape.com/NC-rdf#Name"
#define NC_RDF_FOLDER				"http://home.netscape.com/NC-rdf#Folder"
#define NC_RDF_SPECIALFOLDER		"http://home.netscape.com/NC-rdf#SpecialFolder"
#define NC_RDF_TOTALMESSAGES		"http://home.netscape.com/NC-rdf#TotalMessages"
#define NC_RDF_TOTALUNREADMESSAGES	"http://home.netscape.com/NC-rdf#TotalUnreadMessages"
#define NC_RDF_CHARSET				"http://home.netscape.com/NC-rdf#Charset"
#define NC_RDF_BIFFSTATE			"http://home.netscape.com/NC-rdf#BiffState"

//Folder Commands
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_NEWFOLDER			"http://home.netscape.com/NC-rdf#NewFolder"
#define NC_RDF_GETNEWMESSAGES		"http://home.netscape.com/NC-rdf#GetNewMessages"
#define NC_RDF_COPY					"http://home.netscape.com/NC-rdf#Copy"
#define NC_RDF_MOVE					"http://home.netscape.com/NC-rdf#Move"
#define NC_RDF_MARKALLMESSAGESREAD  "http://home.netscape.com/NC-rdf#MarkAllMessagesRead"
#define NC_RDF_COMPACT				"http://home.netscape.com/NC-rdf#Compact"
#define NC_RDF_RENAME				"http://home.netscape.com/NC_rdf#Rename"
#define NC_RDF_EMPTYTRASH   "http://home.netscape.com/NC_rdf#EmptyTrash"

//Message Commands
#define NC_RDF_MARKREAD				"http://home.netscape.com/NC-rdf#MarkRead"
#define NC_RDF_MARKUNREAD			"http://home.netscape.com/NC-rdf#MarkUnread"
#define NC_RDF_TOGGLEREAD			"http://home.netscape.com/NC-rdf#ToggleRead"


//Returns PR_TRUE if r1 is equal to r2 and r2 is the sort property.
PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isSort);

//Returns PR_TRUE if r1 is equal to r2 and r2 is the collation property.
PRBool
peqCollationSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isCollation);

//Given an nsString, create an nsIRDFNode
nsresult createNode(nsString& str, nsIRDFNode **node);

//Given a PRUint32, create an nsiIRDFNode.
nsresult createNode(PRUint32 value, nsIRDFNode **node);

//Given a PRTime create an nsIRDFNode that is really a date literal.
nsresult createDateNode(PRTime time, nsIRDFNode **node);

//Has Assertion for a datasource that will just call GetTarget on property.  When all of our 
//datasource derive from our datasource baseclass, this should be moved there and the first
//parameter will no longer be needed.
nsresult GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* folderResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion);


