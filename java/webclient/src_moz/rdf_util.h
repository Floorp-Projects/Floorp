/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#ifndef rdf_util_h
#define rdf_util_h

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFContainerUtils.h"
#include "nsIBookmarksService.h"


/**

 * This file contains references to global RDF resources

 */

static const char *BOOKMARKS_URI = "NC:BookmarksRoot";

extern nsCOMPtr<nsIRDFContainerUtils> gRDFCU;
extern nsCOMPtr<nsIRDFService> gRDF;
extern nsCOMPtr<nsIBookmarksService> gBookmarks;
extern nsCOMPtr<nsIRDFDataSource> gBookmarksDataSource;

extern nsCOMPtr<nsIRDFResource> kNC_BookmarksRoot;
extern nsCOMPtr<nsIRDFResource> kNC_Name;
extern nsCOMPtr<nsIRDFResource> kNC_URL;
extern nsCOMPtr<nsIRDFResource> kNC_parent;
extern nsCOMPtr<nsIRDFResource> kNC_Folder;
extern nsCOMPtr<nsIRDFResource> kRDF_type;

extern nsCOMPtr<nsIRDFResource> kNewFolderCommand;
extern nsCOMPtr<nsIRDFResource> kNewBookmarkCommand;

nsresult rdf_startup();
nsresult rdf_shutdown();
void rdf_recursiveResourceTraversal(nsCOMPtr<nsIRDFResource> currentResource);
void rdf_printArcLabels(nsCOMPtr<nsIRDFResource> currentResource);

nsresult rdf_getChildAt(int index, nsIRDFResource *parent,
                        nsIRDFResource **_retval);
nsresult rdf_getChildCount(nsIRDFResource *parent, PRInt32 *count);
nsresult rdf_getIndexOfChild(nsIRDFResource *parent, 
                             nsIRDFResource *child, 
                             PRInt32 *index);


#endif
