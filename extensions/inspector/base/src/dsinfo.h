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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
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

