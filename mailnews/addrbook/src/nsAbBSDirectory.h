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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Paul Sandoz
 */


#ifndef nsAbBSDirectory_h__
#define nsAbBSDirectory_h__

#include "nsRDFResource.h"
#include "nsAbDirProperty.h"
#include "nsHashtable.h"

#include "nsISupportsArray.h"

class nsAbBSDirectory : public nsRDFResource, public nsAbDirProperty
{
public:
	NS_DECL_ISUPPORTS_INHERITED

	nsAbBSDirectory();
	virtual ~nsAbBSDirectory();
	
	// nsIAbDirectory methods
	NS_IMETHOD GetChildNodes(nsIEnumerator* *result);
	NS_IMETHOD CreateNewDirectory(PRUint32 prefCount, const char **prefName, const PRUnichar **prefValue);
	NS_IMETHOD CreateDirectoryByURI(const PRUnichar *dirName, const char *uri, PRBool migrating);
  NS_IMETHOD DeleteDirectory(nsIAbDirectory *directory);
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir);


protected:
	nsresult AddDirectory(const char *uriName, nsIAbDirectory **childDir);
	nsVoidArray* GetDirList(){ return DIR_GetDirectories(); }

	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);
  nsresult CreateDirectoryPAB(const PRUnichar *displayName, const char *fileName,  PRBool migrating);

protected:
	PRBool mInitialized;
	nsCOMPtr<nsISupportsArray> mSubDirectories;
	nsHashtable mServers;
};

#endif
