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
#include "nsIRDFNode.h"
#include "nsString.h"

// this is used for notification of observers using nsVoidArray
typedef struct _nsMsgRDFNotification {
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsMsgRDFNotification;

//Some property declarations
#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Subject);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Sender);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Date);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Status);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, MessageChild);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, SpecialFolder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, TotalMessages);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, TotalUnreadMessages);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Delete);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, NewFolder);


//Returns PR_TRUE if r1 is equal to r2
PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2);

//Returns PR_TRUE if r1 is equal to r2 and r2 is the sort property.
PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isSort);

//Given an nsString, create an nsIRDFNode
void createNode(nsString& str, nsIRDFNode **node);

//Given a PRUint32, create an nsiIRDFNode.
void createNode(PRUint32 value, nsIRDFNode **node);

