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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *             Rajiv Dayal <rdayal@netscape.com>
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

#ifndef nsAbPalmHotSync_h__
#define nsAbPalmHotSync_h__

#include "msgMapi.h"
#include "nsString.h"
#include "nsAbIPCCard.h"
#include "nsIAbCard.h"
#include "nsAddrDatabase.h"
#include "nsILocalFile.h"
#include "nsIAbDirectory.h"

class nsAbPalmHotSync
{
public:

    // this class will do HotSync for a specific AB
    nsAbPalmHotSync(PRBool aIsUnicode, PRUnichar * aAbDescUnicode, char * aAbDesc, PRInt32 aPalmCatID);
    ~nsAbPalmHotSync();

    // initialize the object, info for AB for the object, etc
    nsresult Initialize();

    // this will take in all updated/new Palm records and start Sync.
    // Sync process will load updated/new cards and compare with updated/new 
    // Palm records to create final list to be sent to Palm and to update AB.
    nsresult DoSyncAndGetUpdatedCards(PRInt32 aPalmCount, lpnsABCOMCardStruct aPalmRecords, PRInt32 * aMozCount, lpnsABCOMCardStruct * aMozCards);

    // this will return all cards in an AB
    nsresult GetAllCards(PRInt32 * aCount, lpnsABCOMCardStruct * aCardList);

    // this will return the final list created by above Sync methods
    // allocates aCardList as a memory block and populates it with mIPCCardList to be sent to Palm
    nsresult GetCardsToBeSentToPalm(PRInt32 * aCount, lpnsABCOMCardStruct * aCardList);

    // this will create a new AB and all data into it
    nsresult AddAllRecordsInNewAB(PRInt32 aCount, lpnsABCOMCardStruct aPalmRecords);

    // this will be called when an AckSyncDone is recieved from the Conduit
    nsresult Done(PRBool aSuccess, PRInt32 aPalmCatID, PRUint32 aPalmRecIDListCount = 0, unsigned long * aPalmRecordIDList = nsnull);

protected:

    PRBool mIsPalmDataUnicode;

    // data members related to AB DB
    PRUint32  mTotalCardCount;
    nsCOMPtr<nsIAddrDatabase> mABDB;
    nsCOMPtr <nsILocalFile> mABFile;
    PRBool   mDBOpen;
    // pref for the AB DB
    nsString     mAbName;
    DIR_Server * mDirServerInfo;
    PRInt32      mPalmCategoryId;
    // cards directory for the AB
    nsCOMPtr<nsIAbDirectory> mDirectory;

    // previous AB, the AB state before the last sync
    nsCOMPtr <nsILocalFile> mPreviousABFile;

    // Palm records list
    nsVoidArray mPalmRecords;
    // CardList to be sent to Palm
    PRUint32 mCardForPalmCount;
    lpnsABCOMCardStruct mCardListForPalm;
    // since cards are already in memory because the db is open, 
    // this doesn't add much to memory overhead
    PRUint32 mNewCardCount;
    nsCOMPtr <nsISupportsArray> mNewCardsArray;
    PRBool   mIsNewCardForPalm;
    
    PRBool      mInitialized;
protected:
    // disallow calling of default constructor
    nsAbPalmHotSync();

    // this will take care of opening, nsIAbDirectroy creation, DB backup, etc
    nsresult OpenABDBForHotSync(PRBool aCreate);
    nsresult KeepCurrentStateAsPrevious();
    nsresult UpdateMozABWithPalmRecords();

    // this will load and create a list of cards since last sync
    nsresult LoadNewModifiedCardsSinceLastSync();
    nsresult LoadDeletedCardsSinceLastSync();

    // Checks with the corresponding Palm record for any discrepencies
    // if any modification needed, this will return the modified card
    // returns if card to be kept or discarded completely
    PRBool CheckWithPalmRecord(nsAbIPCCard  * aIPCCard);

    // utility function
    nsresult AddToListForPalm(nsAbIPCCard & ipcCard);
    void ConvertAssignPalmIDAttrib(PRUint32 id, nsIAbMDBCard * card);

};

#endif
