/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include"PlugletsDir.h"
#include "prenv.h"
#include "PlugletLog.h"
#include "nsFileSpec.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"

static PRIntn DeleteEntryEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
    PLHashTable *hash = (PLHashTable *) arg;

    char *key = (char *) he->key;
    
    PL_strfree(key);

    PlugletFactory *toDelete = (PlugletFactory *) he->value;
    delete toDelete;

    PL_HashTableRemove(hash, he->key);
    return 0;
}

PlugletsDir::PlugletsDir(void) : mMimeTypeToPlugletFacoryHash(nsnull) {
}

PlugletsDir::~PlugletsDir(void) {
    if (mMimeTypeToPlugletFacoryHash) {
	PL_HashTableEnumerateEntries(mMimeTypeToPlugletFacoryHash,
				     DeleteEntryEnumerator, 
				     mMimeTypeToPlugletFacoryHash);
	PL_HashTableDestroy(mMimeTypeToPlugletFacoryHash);
	mMimeTypeToPlugletFacoryHash = nsnull;
    }
}

nsresult PlugletsDir::LoadPluglets() {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletsDir::LoadPluglets\n"));
    const char *mimeType = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    if (!mMimeTypeToPlugletFacoryHash) {
	mMimeTypeToPlugletFacoryHash = 
	    PL_NewHashTable(20, PL_HashString, 
			    PL_CompareStrings, 
			    PL_CompareValues, 
			    nsnull, nsnull);
	char * path = PR_GetEnv("PLUGLET");
	if (!path) {
	  nsCOMPtr<nsIFile> sysDir;
	  nsresult rv = NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR,
					       getter_AddRefs(sysDir));
          if (NS_FAILED(rv)) {
	    return rv;
	  }

	  rv = sysDir->AppendNative(NS_LITERAL_CSTRING("plugins"));
	  if (NS_FAILED(rv)) {
	    return rv;
	  }

	  nsXPIDLCString pluginsDir;
	  rv = sysDir->GetNativePath(pluginsDir);
	  if (NS_FAILED(rv)) {
	    return rv;
	  }

	  path = PL_strdup(pluginsDir.get());
	}
	if (!path) {
	    return rv;
	}
	PlugletFactory *plugletFactory;
	nsFileSpec dir(path);
	for (nsDirectoryIterator iter(dir,PR_TRUE); iter.Exists(); iter++) {
	    const nsFileSpec& file = iter;
	    const char* name = file.GetCString();
	    int length;
	    if((length = strlen(name)) <= 4  // if it's shorter than ".jar"
	       || strcmp(name+length - 4,".jar") ) {
		continue;
	    }
	    if ( (plugletFactory = PlugletFactory::Load(name)) ) {
		mimeType = nsnull;
		rv = plugletFactory->GetMIMEDescription(&mimeType);
		if (NS_SUCCEEDED(rv)) {
		    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
			   ("PlugletsDir::LoadPluglets: got mime description: %s\n", mimeType));
		    
		    const char *key = PL_strdup(mimeType);
		    PLHashEntry *entry = 
			PL_HashTableAdd(mMimeTypeToPlugletFacoryHash,
					(const void *) key, 
					plugletFactory);
		    rv = (nsnull != entry) ? NS_OK : NS_ERROR_FAILURE;
		    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
			   ("PlugletsDir::LoadPluglets: adding to mimeTypeToPlugletFactoryHash. rv: %d\n", rv));
		}
	    }
	}

	if (path) {
	    PL_strfree(path);
	}
    }
    return rv;
}

nsresult PlugletsDir::GetPlugletFactory(const char * mimeType, PlugletFactory **plugletFactory) {
    if(!plugletFactory) {
	return NS_ERROR_NULL_POINTER;
    }
    *plugletFactory = nsnull;
    PlugletFactory *cur = nsnull;
    nsresult res = NS_ERROR_FAILURE;
    if(!mMimeTypeToPlugletFacoryHash) {
	res = LoadPluglets();
    }
    else {
	res = NS_OK;
    }
    if (NS_SUCCEEDED(res) && mMimeTypeToPlugletFacoryHash) {
	*plugletFactory = (PlugletFactory *) 
	    PL_HashTableLookup(mMimeTypeToPlugletFacoryHash,
			       (const void *)mimeType);
	printf("debug: edburns: %p\n", *plugletFactory);
	res = (nsnull != *plugletFactory) ? NS_OK : NS_ERROR_FAILURE;
    }
    return res;
}

