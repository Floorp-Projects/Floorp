/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
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
#ifndef nsAbOutlookDirectory_h___
#define nsAbOutlookDirectory_h___

#include "nsAbDirectoryRDFResource.h"
#include "nsAbDirProperty.h"
#include "nsIAbDirectoryQuery.h"
#include "nsIAbDirectorySearch.h"
#include "nsAbDirSearchListener.h"
#include "nsHashtable.h"

#include "nsISupportsArray.h"

struct nsMapiEntry ;

class nsAbOutlookDirectory : public nsAbDirectoryRDFResource, // nsIRDFResource
                             public nsAbDirProperty, // nsIAbDirectory
                             public nsIAbDirectoryQuery,
                             public nsIAbDirectorySearch,
                             public nsAbDirSearchListenerContext
{
public:
	NS_DECL_ISUPPORTS_INHERITED

	nsAbOutlookDirectory(void) ;
	virtual ~nsAbOutlookDirectory(void) ;
	
	// nsAbDirProperty methods
	NS_IMETHOD GetChildCards(nsIEnumerator **aCards) ;
	NS_IMETHOD GetChildNodes(nsIEnumerator **aNodes) ;
    NS_IMETHOD HasCard(nsIAbCard *aCard, PRBool *aHasCard) ;
    NS_IMETHOD HasDirectory(nsIAbDirectory *aDirectory, PRBool *aHasDirectory) ;
    NS_IMETHOD DeleteCards(nsISupportsArray *aCardList) ;
    NS_IMETHOD DeleteDirectory(nsIAbDirectory *aDirectory) ;
    NS_IMETHOD AddCard(nsIAbCard *aData, nsIAbCard **aCard) ;
    NS_IMETHOD DropCard(nsIAbCard *aData, nsIAbCard **aCard) ;
    NS_IMETHOD AddMailList(nsIAbDirectory *aMailList) ;
    NS_IMETHOD EditMailListToDatabase(const char *aUri) ;
    NS_IMETHOD GetTotalCards(PRBool aSubDirectoryCount, PRUint32 *aNbCards) ;
    // nsAbDirectoryRDFResource method
    NS_IMETHOD Init(const char *aUri) ;
    // nsIAbDirectoryQuery methods
    NS_DECL_NSIABDIRECTORYQUERY
    // nsIAbDirectorySearch methods
    NS_DECL_NSIABDIRECTORYSEARCH
    // nsAbDirSearchListenerContext methods
    nsresult OnSearchFinished(PRInt32 aResult) ;
    nsresult OnSearchFoundCard(nsIAbCard *aCard) ;
    // Perform a MAPI query (function executed in a separate thread)
    nsresult ExecuteQuery(nsIAbDirectoryQueryArguments *aArguments,
                          nsIAbDirectoryQueryResultListener *aListener,
                          PRInt32 aResultLimit, PRInt32 aTimeout,
                          PRInt32 aThreadId) ;

protected:
    // Retrieve hierarchy as cards, with an optional restriction
    nsresult GetChildCards(nsISupportsArray **aCards, void *aRestriction) ;
    // Retrieve hierarchy as directories
    nsresult GetChildNodes(nsISupportsArray **aNodes) ;
    // Create a new card
    nsresult CreateCard(nsIAbCard *aData, nsIAbCard **aNewCard) ;
    // Notification for the UI
    nsresult NotifyItemDeletion(nsISupports *aItem) ;
    nsresult NotifyItemAddition(nsISupports *aItem) ;
    // Force update of MAPI repository for mailing list
    nsresult CommitAddressList(void) ;
    // Read MAPI repository
    nsresult UpdateAddressList(void) ;

    nsMapiEntry *mMapiData ;
    // Container for the query threads
    nsHashtable mQueryThreads ;
    PRInt32 mCurrentQueryId ;
    PRLock *mProtector ;
    // Data for the search interfaces
	nsSupportsHashtable mCardList ;
    PRInt32 mSearchContext ;
    // Windows AB type
    PRUint32 mAbWinType ;

private:
};

#endif // nsAbOutlookDirectory_h___
