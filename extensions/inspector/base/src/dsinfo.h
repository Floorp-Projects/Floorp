/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#ifndef __dsinfo_h__
#define __dsinfo_h__
 
#include "nsRDFCID.h"
#include "nsHashtable.h"

#define IN_NAMESPACE_URI "http://www.mozilla.org/inspector#"

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kISupportsIID,               NS_ISUPPORTS_IID);

// This might not be thread safe as a global variable, but it needs to be global
// because multiple instances of nsDOMDataSource need to use the same
// resource objects for the same nodes so they can pass resource id's
// back and forth.  Need to make sure this is thread-safe in the future, though.
static nsSupportsHashtable gDOMObjectTable;

#endif // __dsinfo_h__

