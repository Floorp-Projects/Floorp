/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *       Rajiv Dayal <rdayal@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsAbPalmSync.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"
#include "nsMsgI18N.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsUnicharUtils.h"
#include "nsAbMDBCard.h"
#include "nsAbCardProperty.h"
#include "prdtoa.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define  PREVIOUS_EXTENSION ".prev"

#ifdef RAJIV_DEBUG
   PRBool PalmDataDisplayed = PR_FALSE;
   void DisplayTestData(nsABCOMCardStruct * aIPCCard, PRBool IsUnicode)
   {
       if (IsUnicode) {
           if(aIPCCard->displayName)
               printf("\n Card Data For : %S:", aIPCCard->displayName);
           if(aIPCCard->firstName)
               printf("\n First Name : %S", aIPCCard->firstName);
           if(aIPCCard->lastName)
               printf("\n Last Name : %S", aIPCCard->lastName);
           if(aIPCCard->primaryEmail)
               printf("\n Email : %S", aIPCCard->primaryEmail);
           if(aIPCCard->workPhone)
               printf("\n Work Phone : %S", aIPCCard->workPhone);
           if(aIPCCard->homePhone)
               printf("\n Home Phone : %S", aIPCCard->homePhone);
           if(aIPCCard->faxNumber)
               printf("\n Fax Number : %S", aIPCCard->faxNumber);
           if(aIPCCard->company)
               printf("\n Company : %S", aIPCCard->company);
           if(aIPCCard->jobTitle)
               printf("\n Job Title : %S", aIPCCard->jobTitle);
           if(aIPCCard->homeAddress)
               printf("\n Home Address : %S", aIPCCard->homeAddress);
           if(aIPCCard->homeCity)
               printf("\n Home City : %S", aIPCCard->homeCity);
           if(aIPCCard->homeState)
               printf("\n Home State : %S", aIPCCard->homeState);
           printf("\n");
       }
       else {
           if(aIPCCard->displayName)
               printf("\n Card Data For : %s:", (char *) aIPCCard->displayName);
           if(aIPCCard->firstName)
               printf("\n First Name : %s", (char *) aIPCCard->firstName);
           if(aIPCCard->lastName)
               printf("\n Last Name : %s", (char *) aIPCCard->lastName);
           if(aIPCCard->primaryEmail)
               printf("\n Email : %s", (char *) aIPCCard->primaryEmail);
           if(aIPCCard->workPhone)
               printf("\n Work Phone : %s", (char *) aIPCCard->workPhone);
           if(aIPCCard->homePhone)
               printf("\n Home Phone : %s", (char *) aIPCCard->homePhone);
           if(aIPCCard->faxNumber)
               printf("\n Fax Number : %s", (char *) aIPCCard->faxNumber);
           if(aIPCCard->company)
               printf("\n Company :   %s", (char *) aIPCCard->company);
           if(aIPCCard->jobTitle)
               printf("\n Job Title : %s", (char *) aIPCCard->jobTitle);
           if(aIPCCard->homeAddress)
               printf("\n Home Address : %s", (char *) aIPCCard->homeAddress);
           if(aIPCCard->homeCity)
               printf("\n Home City : %s", (char *) aIPCCard->homeCity);
           if(aIPCCard->homeState)
               printf("\n Home State : %s", (char *) aIPCCard->homeState);
           printf("\n");
       }
   }
#endif

nsAbPalmHotSync::nsAbPalmHotSync(PRBool aIsUnicode, PRUnichar * aAbDescUnicode, char * aAbDesc, PRInt32 aPalmCatID)
{
    mTotalCardCount=0;

    mCardForPalmCount = 0;
    mCardListForPalm = nsnull;
    mDirServerInfo = nsnull;
    
    mInitialized = PR_FALSE;
    mDBOpen = PR_FALSE;
    
    if(aIsUnicode)
        mAbName.Assign(aAbDescUnicode);
    else
        // mAbName.AssignWithConversion(aAbDesc);
        mAbName = NS_ConvertUTF8toUCS2(aAbDesc);
    mAbName.Trim(" ");

    mPalmCategoryId = aPalmCatID;

    mIsPalmDataUnicode = aIsUnicode;

    mNewCardCount = 0;
    NS_NewISupportsArray(getter_AddRefs(mNewCardsArray));
    mIsNewCardForPalm = PR_FALSE;
}

nsAbPalmHotSync::~nsAbPalmHotSync()
{
    // clear the nsVoidArray, donot free the stored pointers since it is freed by calling app (Conduit)
    mPalmRecords.Clear();

    if(mDBOpen && mABDB)
        mABDB->Close(PR_FALSE);
}

// this is a utility function
void nsAbPalmHotSync::ConvertAssignPalmIDAttrib(PRUint32 id, nsIAbMDBCard * card)  
{ 
    PRInt64 l;
    LL_UI2L(l, id);
    PRFloat64 f;
    LL_L2F(f, l);
    char buf[128];
    PR_cnvtf(buf, 128, 0, f);
    card->SetAbDatabase(mABDB);
    card->SetStringAttribute(CARD_ATTRIB_PALMID,NS_ConvertASCIItoUCS2(buf).get());
}

nsresult nsAbPalmHotSync::Initialize()
{
    nsresult rv = NS_OK;
    
    if(mInitialized)
        return rv;

    if (!DIR_GetDirectories())
        return NS_ERROR_FAILURE;

    PRInt32 count = DIR_GetDirectories()->Count();

    DIR_Server *server = nsnull;
    for (PRInt32 i = 0; i < count; i++) {
        server = (DIR_Server *)(DIR_GetDirectories()->ElementAt(i));

        // if this is a 4.x, local .na2 addressbook (PABDirectory)
        // we must skip it.
        PRUint32 fileNameLen = strlen(server->fileName);
        if (((fileNameLen > kABFileName_PreviousSuffixLen) && 
                strcmp(server->fileName + fileNameLen - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0) &&
                (server->dirType == PABDirectory))
            continue;

        // if Palm category is already assigned to AB then just check that
        if((server->PalmCategoryId > -1) && (mPalmCategoryId == server->PalmCategoryId))
            break;

        // convert to unicode
        nsAutoString abName;
        nsCAutoString platformCharSet(nsMsgI18NFileSystemCharset());
        rv = ConvertToUnicode(platformCharSet.get(), server->description, abName);
        if(NS_FAILED(rv)) return rv ;

        // if Palm category is not already assigned check the AB name
        if(abName.Find(mAbName) != kNotFound)
            break;

        server = nsnull;
    }

    if(!server || !server->fileName)
        return NS_ERROR_FILE_NOT_FOUND;

    mDirServerInfo = server;

    mInitialized = TRUE;

    return NS_OK;
}

nsresult nsAbPalmHotSync::AddAllRecordsInNewAB(PRInt32 aCount, lpnsABCOMCardStruct aPalmRecords)
{
    NS_ENSURE_ARG_POINTER(aPalmRecords);

    DIR_Server * server = nsnull;
    nsAutoString fileName(mAbName.get());
    fileName.AppendWithConversion(".mab");
    fileName.StripWhitespace();
    nsresult rv = DIR_AddNewAddressBook(mAbName.get(), NS_ConvertUCS2toUTF8(fileName).get(),
                                            PR_FALSE, MAPIDirectory, &server);
    if(NS_FAILED(rv))
        return rv;

    mDirServerInfo = server;
    mInitialized = TRUE;

    // open the Moz AB database
    rv = OpenABDBForHotSync(PR_TRUE);
    if(NS_FAILED(rv))
        return rv;

    // we are just storing the pointer array here not record arrays
    for (PRInt32 i=0; i < aCount; i++)
        mPalmRecords.AppendElement(&aPalmRecords[i]);

    // new DB here so no need to backup
    rv = UpdateMozABWithPalmRecords();

    rv = mABDB->Close(NS_SUCCEEDED(rv));

    if(NS_SUCCEEDED(rv)) {
        mDBOpen = PR_FALSE;
        PRUint32 modTimeInSec;
        nsAddrDatabase::PRTime2Seconds(PR_Now(), &modTimeInSec);
        mDirServerInfo->PalmSyncTimeStamp = modTimeInSec;
        mDirServerInfo->PalmCategoryId = mPalmCategoryId;
        DIR_SavePrefsForOneServer(mDirServerInfo);
    }
    else { // get back the previous file
        rv = mABDB->ForceClosed();
        if(NS_SUCCEEDED(rv)) {
            nsCAutoString leafName;
            mABFile->GetNativeLeafName(leafName);
            PRBool bExists=PR_FALSE;
            mPreviousABFile->Exists(&bExists);
            if(bExists) {
                nsCOMPtr<nsIFile> parent;
                rv = mABFile->GetParent(getter_AddRefs(parent));
                if (NS_SUCCEEDED(rv)) {
                    mABFile->Remove(PR_FALSE);
                    mPreviousABFile->CopyToNative(parent, leafName);
                }
            }
            mDBOpen = PR_FALSE;
        }
    }

    return rv;
}

nsresult nsAbPalmHotSync::GetAllCards(PRInt32 * aCount, lpnsABCOMCardStruct * aCardList)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aCardList);

    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // open the Moz AB database
    nsresult rv = OpenABDBForHotSync(PR_FALSE); 
    if(NS_FAILED(rv))
        return rv;

    // create the list to be sent back to Palm
    mABDB->GetCardCount(&mTotalCardCount);
    *aCount = mTotalCardCount;
    if(!mTotalCardCount) {
        mABDB->Close(PR_FALSE);
        return NS_OK;
    }

    mCardListForPalm = (lpnsABCOMCardStruct) CoTaskMemAlloc(sizeof(nsABCOMCardStruct) * mTotalCardCount);
    if(!mCardListForPalm) {
        mABDB->Close(PR_FALSE);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIEnumerator> cardsEnumerator;
    rv = mABDB->EnumerateCards(mDirectory, getter_AddRefs(cardsEnumerator));
    if (NS_FAILED(rv) || !cardsEnumerator) {
        mABDB->Close(PR_FALSE);
        return rv;
    }

    nsCOMPtr<nsISupports> item;
    nsCOMPtr<nsIAbCard> card;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next()) {
        rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
        if (NS_FAILED(rv)) 
            continue;

        card = do_QueryInterface(item, &rv);
        if (NS_FAILED(rv)) // can this be anything but a card?
            continue;

        PRBool isMailingList=PR_FALSE;
        rv = card->GetIsMailList(&isMailingList);
        if (NS_FAILED(rv) || isMailingList) // if mailing list go to cards
            continue;

        nsAbIPCCard  ipcCard(card);
        ipcCard.SetStatus(ATTR_NEW);

        rv = AddToListForPalm(ipcCard);
        if(NS_FAILED(rv))
            break;

        mNewCardsArray->AppendElement(card);
        mNewCardCount++;
    }


    *aCount = mCardForPalmCount;
    *aCardList = mCardListForPalm;

    // Don't close the DB yet, it will be closed when Done is called

    return NS_OK;
}

nsresult nsAbPalmHotSync::DoSyncAndGetUpdatedCards(PRInt32 aPalmCount, lpnsABCOMCardStruct aPalmRecords, 
                                                    PRInt32 * aMozCount, lpnsABCOMCardStruct * aMozCards)
{
    NS_ENSURE_ARG_POINTER(aPalmRecords);
    NS_ENSURE_ARG_POINTER(aMozCount);
    NS_ENSURE_ARG_POINTER(aMozCards);

    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // open the Moz AB database
    nsresult rv = OpenABDBForHotSync(PR_FALSE); 
    if(NS_FAILED(rv))
        return rv;

    // we are just storing the pointer array here not record arrays
    for (PRInt32 i=0; i < aPalmCount; i++) {
        mPalmRecords.AppendElement(&aPalmRecords[i]);
#ifdef RAJIV_DEBUG        
        DisplayTestData(&aPalmRecords[i],PR_FALSE);
#endif
    }

    // create the list to be sent back to Palm
    mABDB->GetCardCount(&mTotalCardCount);
    PRUint32  deletedCardCount=0;
    mABDB->GetDeletedCardCount(&deletedCardCount);
    mTotalCardCount += deletedCardCount;
    if(mTotalCardCount) {
        mCardListForPalm = (lpnsABCOMCardStruct) CoTaskMemAlloc(sizeof(nsABCOMCardStruct) * mTotalCardCount);
        if(!mCardListForPalm) {
            mABDB->Close(PR_FALSE);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    // initialize the flag for any new Moz cards to be added to Palm
    mIsNewCardForPalm = PR_FALSE;

    // deal with new / modified or first time sync
    rv = LoadNewModifiedCardsSinceLastSync();
    if(NS_SUCCEEDED(rv))
        // deal with deleted
        rv = LoadDeletedCardsSinceLastSync();
    if(NS_SUCCEEDED(rv))
        // first backup the existing DB as Previous
        rv = KeepCurrentStateAsPrevious();
    if(NS_SUCCEEDED(rv))
        // update local DB for sync
        rv = UpdateMozABWithPalmRecords();

    // if there are no new cards to be added in Palm close DB
    // else wait for Done to be called.
    if(!mIsNewCardForPalm) {
        rv = mABDB->Close(NS_SUCCEEDED(rv) && mPalmRecords.Count());

        if(NS_SUCCEEDED(rv)) {
            mDBOpen = PR_FALSE;
            PRUint32 modTimeInSec;
            nsAddrDatabase::PRTime2Seconds(PR_Now(), &modTimeInSec);
            mDirServerInfo->PalmSyncTimeStamp = modTimeInSec;
            mDirServerInfo->PalmCategoryId = mPalmCategoryId;
            DIR_SavePrefsForOneServer(mDirServerInfo);
        }
        else { // get back the previous file
            rv = mABDB->ForceClosed();
            if(NS_SUCCEEDED(rv)) {
                nsCAutoString leafName;
                mABFile->GetNativeLeafName(leafName);
                PRBool bExists=PR_FALSE;
                mPreviousABFile->Exists(&bExists);
                if(bExists) {
                    nsCOMPtr<nsIFile> parent;
                    rv = mABFile->GetParent(getter_AddRefs(parent));
                    if (NS_SUCCEEDED(rv)) {
                        mABFile->Remove(PR_FALSE);
                        mPreviousABFile->CopyToNative(parent, leafName);
                    }
                }
                mDBOpen = PR_FALSE;
            }
        }
    }

    *aMozCount = mCardForPalmCount;
    *aMozCards = mCardListForPalm;

    return rv;
}

// this takes care of the cases when the state is deleted
nsresult nsAbPalmHotSync::LoadDeletedCardsSinceLastSync()
{
    if(!mDBOpen || !mABDB || !mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;
    
    nsISupportsArray * deletedCardArray;
    PRUint32  deletedCardCount;
    nsresult rv = mABDB->GetDeletedCardList(&deletedCardCount, &deletedCardArray);
    if(NS_FAILED(rv))
        return rv;

    for(PRUint32 i=0; i < deletedCardCount; i++) 
    {
        nsCOMPtr<nsIAbCard> card; 
        rv = deletedCardArray->QueryElementAt(i, NS_GET_IID(nsIAbCard), getter_AddRefs(card));
        if (NS_FAILED(rv)) // can this be anything but a card?
            continue;
        
        PRBool isMailingList=PR_FALSE;
        rv = card->GetIsMailList(&isMailingList);
        if (NS_FAILED(rv) || isMailingList)
            continue;

        PRUint32 lastModifiedDate = 0;
        rv = card->GetLastModifiedDate(&lastModifiedDate);
        if (NS_FAILED(rv) || !lastModifiedDate)
            continue;

        if(lastModifiedDate > mDirServerInfo->PalmSyncTimeStamp) {
            nsAbIPCCard  ipcCard(card);
            // check in the list of Palm records
            for(PRInt32 i=mPalmRecords.Count()-1; i >=0;  i--) {
                nsABCOMCardStruct * palmRec = (nsABCOMCardStruct *) mPalmRecords.ElementAt(i);
                // if same record exists in palm list, donot delete it from Palm
                if(ipcCard.Same(palmRec, mIsPalmDataUnicode))
                    continue;
            }
            ipcCard.SetStatus(ATTR_DELETED);

            rv = AddToListForPalm(ipcCard);
            if(NS_FAILED(rv))
                break;
        }
    }
    return rv;
}


nsresult nsAbPalmHotSync::LoadNewModifiedCardsSinceLastSync()
{
    if(!mDBOpen || !mABDB || !mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIEnumerator> cardsEnumerator;
    nsresult rv = mABDB->EnumerateCards(mDirectory, getter_AddRefs(cardsEnumerator));
    if (NS_FAILED(rv) || !cardsEnumerator) 
        return NS_ERROR_NOT_AVAILABLE; // no cards available

    // create the list of cards to be sent to Palm
    nsCOMPtr<nsISupports> item;
    nsCOMPtr<nsIAbCard> card;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next()) {
        rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
        if (NS_FAILED(rv)) 
            return rv;

        card = do_QueryInterface(item, &rv);
        if (NS_FAILED(rv)) // can this be anything but a card?
            continue;

        PRBool isMailingList=PR_FALSE;
        rv = card->GetIsMailList(&isMailingList);
        if (NS_FAILED(rv) || isMailingList) // if mailing list go to cards
            continue;

        PRUint32 lastModifiedDate = 0;
        rv = card->GetLastModifiedDate(&lastModifiedDate);
        if (NS_FAILED(rv))
            continue;

        if(lastModifiedDate > mDirServerInfo->PalmSyncTimeStamp  // take care of modified
            || !lastModifiedDate || !mDirServerInfo->PalmSyncTimeStamp) {  // take care of new or never before sync
            
            //create nsAbIPCCard and assign its status based on lastModifiedDate
            nsAbIPCCard ipcCard(card);
            ipcCard.SetStatus((lastModifiedDate) ? ATTR_MODIFIED : ATTR_NEW);

            // Check with the palm list (merging is done if required)
            if (!CheckWithPalmRecord(&ipcCard)) 
                continue;

            rv = AddToListForPalm(ipcCard);
            if(NS_FAILED(rv))
                break;

            if(ipcCard.GetStatus() == ATTR_NEW) {
                mNewCardsArray->AppendElement(card);
                mNewCardCount++;
                mIsNewCardForPalm = PR_TRUE;
            }
        }
    }

    return NS_OK;
}


// this take care of the all cases when the state is either mod or new
PRBool nsAbPalmHotSync::CheckWithPalmRecord(nsAbIPCCard  * aIPCCard)
{
    NS_ENSURE_ARG_POINTER(aIPCCard);

    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    PRBool keep = PR_TRUE;

    for(PRInt32 i=mPalmRecords.Count()-1; i >=0;  i--) {
        nsABCOMCardStruct * palmRec = (nsABCOMCardStruct *) mPalmRecords.ElementAt(i);
        
        // if same records exists in palm list also
        if(aIPCCard->Same(palmRec, mIsPalmDataUnicode)) {
            // if the state deleted on both sides no need to do anything
            if ((palmRec->dwStatus == ATTR_DELETED) && (aIPCCard->GetStatus() == ATTR_DELETED)) {
                mPalmRecords.RemoveElementAt(i);
                return PR_FALSE;
            }
            // if deleted on Palm and added or modified on Moz, donot delete on Moz
            if ( (palmRec->dwStatus == ATTR_DELETED) && 
                 ((aIPCCard->GetStatus() == ATTR_NEW) 
                   ||(aIPCCard->GetStatus() == ATTR_MODIFIED)) ) {
                mPalmRecords.RemoveElementAt(i);
                return PR_TRUE;
            }

            // set the palm record ID in the card if not already set
            if(!aIPCCard->GetRecordId()) {
                ConvertAssignPalmIDAttrib(palmRec->dwRecordId, aIPCCard);
                mABDB->EditCard(aIPCCard, PR_FALSE);
                aIPCCard->SetRecordId(palmRec->dwRecordId);
            }

            // if deleted in Moz and added or modified on Palm, donot delete on Palm
            if ( (aIPCCard->GetStatus() == ATTR_DELETED) && 
                 ((palmRec->dwStatus == ATTR_NEW) 
                   ||(palmRec->dwStatus == ATTR_MODIFIED)) ) {
                return PR_FALSE;
            }

            // for rest of the cases with state as either mod or new::
            nsStringArray diffAttrs;
            PRBool isFieldsMatch=PR_FALSE;
            if(mIsPalmDataUnicode)
                isFieldsMatch = aIPCCard->Equals(palmRec, diffAttrs);
            else
                isFieldsMatch = aIPCCard->EqualsAfterUnicodeConversion(palmRec, diffAttrs);

            // if fields are ditto no need to keep it for sync
            if(isFieldsMatch) {
                keep = PR_FALSE;
                // since the fields are ditto no need to update Moz AB with palm record
                mPalmRecords.RemoveElementAt(i); 
            }
            else {
                // we add an additional record on both sides alike Palm Desktop sync
                palmRec->dwStatus = ATTR_NONE;
                aIPCCard->SetStatus(ATTR_NEW);
                keep = PR_TRUE;
            }
            // since we found the same record in palm sync list
            // no need to go thru the rest of the mPalmRecords list
            break;
        }
    }

    return keep;
}

// utility function
nsresult nsAbPalmHotSync::AddToListForPalm(nsAbIPCCard & ipcCard)
{
    // this should never happen but check for crossing array index
    if(mCardForPalmCount >= mTotalCardCount)
        return NS_ERROR_UNEXPECTED;

    nsresult rv = ipcCard.GetABCOMCardStruct(mIsPalmDataUnicode, &mCardListForPalm[mCardForPalmCount]);
    if(NS_SUCCEEDED(rv)) {
        mCardListForPalm[mCardForPalmCount].dwCategoryId = mPalmCategoryId;
        mCardForPalmCount++;
    }

    return rv;
}

nsresult nsAbPalmHotSync::OpenABDBForHotSync(PRBool aCreate)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // if already open
    if(mDBOpen && mABDB && mDirectory)
        return NS_OK;

    if(!mDirServerInfo->fileName)
        return NS_ERROR_FILE_INVALID_PATH;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
    if(NS_FAILED(rv)) 
        return rv;

    nsFileSpec* dbPath;
    rv = abSession->GetUserProfileDirectory(&dbPath);
    if(NS_FAILED(rv)) 
        return rv;

    (*dbPath) += mDirServerInfo->fileName;

    // get nsIFile for nsFileSpec from abSession, why use a obsolete class if not required!
    rv = NS_FileSpecToIFile(dbPath, getter_AddRefs(mABFile));
    if(NS_FAILED(rv))  {
        delete dbPath;
        return rv;
    }

    nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
             do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
    if(NS_FAILED(rv)) {
        delete dbPath;
        return rv;
    }
    
    rv = addrDBFactory->Open(dbPath, aCreate, getter_AddRefs(mABDB), PR_TRUE);
    delete dbPath;
    if(NS_FAILED(rv)) {
        return rv;
    }

    mDBOpen = PR_TRUE;  // Moz AB DB is now Open

    // This is in case the uri is never set
    // in the nsDirPref.cpp code.
    if (!mDirServerInfo->uri) {
        nsCAutoString URI;        
        URI = NS_LITERAL_CSTRING(kMDBDirectoryRoot) + nsDependentCString(mDirServerInfo->fileName);
        mDirServerInfo->uri = ToNewCString(URI);
    }

    // Get the RDF service...
    nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
    if (NS_FAILED(rv)) 
        return rv;

    nsCOMPtr<nsIRDFResource> resource;
    if ((strncmp(mDirServerInfo->uri, "ldap:", 5) == 0) ||
    (strncmp(mDirServerInfo->uri, "ldaps:", 6) == 0)) {
        nsCAutoString bridgeURI;
        bridgeURI = NS_LITERAL_CSTRING(kLDAPDirectoryRoot) + nsDependentCString(mDirServerInfo->prefName);
        rv = rdfService->GetResource(bridgeURI.get(), getter_AddRefs(resource));
        if (NS_FAILED(rv)) 
            return rv;
    }
    else {
        rv = rdfService->GetResource(mDirServerInfo->uri, getter_AddRefs(resource));
        if (NS_FAILED(rv)) 
            return rv;
    }

    // get the directory of cards for the Moz AB
    mDirectory = do_QueryInterface(resource, &rv);

    return rv;
}

nsresult nsAbPalmHotSync::KeepCurrentStateAsPrevious()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    nsCAutoString previousLeafName(mDirServerInfo->fileName);
    previousLeafName += PREVIOUS_EXTENSION;

    if(!mPreviousABFile) {
        nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
        if(NS_FAILED(rv)) 
            return rv;
        nsFileSpec* dbPath;
        rv = abSession->GetUserProfileDirectory(&dbPath); // this still uses nsFileSpec!!!
        if(NS_SUCCEEDED(rv)) {
            (*dbPath) += previousLeafName.get();
            // get nsIFile for nsFileSpec from abSession, why use a obsolete class if not required!
            rv = NS_FileSpecToIFile(dbPath, getter_AddRefs(mPreviousABFile));
            delete dbPath;
            if(NS_FAILED(rv))
                return rv;
        }
    }

    PRBool bExists=PR_FALSE;
    mABFile->Exists(&bExists);
    if(bExists) {
        mPreviousABFile->Exists(&bExists);
        if(bExists)
            rv = mPreviousABFile->Remove(PR_FALSE);
        nsCOMPtr<nsIFile> parent;
        rv = mABFile->GetParent(getter_AddRefs(parent));
        if (NS_SUCCEEDED(rv))
            rv = mABFile->CopyToNative(parent, previousLeafName);
    }        
    return rv;
}


nsresult nsAbPalmHotSync::UpdateMozABWithPalmRecords()
{
    if(!mInitialized || !mABDB || !mDBOpen)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    for(PRInt32 i=mPalmRecords.Count()-1; i >=0;  i--) {
        nsABCOMCardStruct * palmRec = (nsABCOMCardStruct *)mPalmRecords.ElementAt(i);
        nsAbIPCCard ipcCard(palmRec, PR_FALSE);

        char recordIDBuf[128]; 
        PRInt64 l;
        LL_UI2L(l, palmRec->dwRecordId);
        PRFloat64 f;
        LL_L2F(f, l);
        PR_cnvtf(recordIDBuf, 128, 0, f);

        // if the card already exist
        nsCOMPtr<nsIAbCard> existingCard;
        rv = mABDB->GetCardFromAttribute(nsnull, CARD_ATTRIB_PALMID, recordIDBuf,
                                             PR_FALSE, getter_AddRefs(existingCard));
        if(NS_SUCCEEDED(rv) && existingCard) {
            if(palmRec->dwStatus == ATTR_DELETED) {
                mABDB->DeleteCard(existingCard, PR_FALSE);
                continue;
            }
            if(palmRec->dwStatus == ATTR_NEW)
                continue;
            if(palmRec->dwStatus == ATTR_MODIFIED) {
                PRBool isEqual=PR_FALSE;
                ipcCard.Equals(existingCard, &isEqual);
                if(isEqual)
                    continue;
                else {
                    existingCard->Copy(&ipcCard);
                    rv = mABDB->EditCard(existingCard, PR_FALSE);
                    continue;
                }
            }
        }

        nsCOMPtr<nsIAbMDBCard> dbCard;
        dbCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
        if(NS_FAILED(rv))
            continue;

        nsCOMPtr<nsIAbCard> newCard;
        newCard = do_QueryInterface(dbCard, &rv);
        if(NS_FAILED(rv)) 
            continue;

        rv = newCard->Copy(&ipcCard);
        if(NS_FAILED(rv)) 
            continue;

        // if the card does not exist
        if((ipcCard.GetStatus() == ATTR_NEW)
            ||(ipcCard.GetStatus() == ATTR_MODIFIED)
            || (ipcCard.GetStatus() == ATTR_NONE)) {
            PRUint32 modTimeInSec;
            nsAddrDatabase::PRTime2Seconds(PR_Now(), &modTimeInSec);
            ipcCard.SetLastModifiedDate(modTimeInSec);
            rv = mABDB->CreateNewCardAndAddToDB(newCard, PR_FALSE);
            if(NS_SUCCEEDED(rv)) {
                // now set the attribute for the PalmRecID in the card in the DB
                dbCard->SetAbDatabase(mABDB);
                dbCard->SetStringAttribute(CARD_ATTRIB_PALMID, NS_ConvertASCIItoUCS2(recordIDBuf).get());
                newCard = do_QueryInterface(dbCard, &rv);
                if(NS_SUCCEEDED(rv))
                    rv = mABDB->EditCard(newCard, PR_FALSE);
            }
        }
    }

    return rv;
}


nsresult nsAbPalmHotSync::Done(PRBool aSuccess, PRInt32 aPalmCatID, PRUint32 aPalmRecIDListCount, unsigned long * aPalmRecordIDList)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv=NS_ERROR_UNEXPECTED;

    if(aPalmRecIDListCount == mNewCardCount) {
        for(PRUint32 i=0; i<aPalmRecIDListCount; i++) {
            nsCOMPtr<nsIAbMDBCard> dbCard;
            rv = mNewCardsArray->QueryElementAt(i, NS_GET_IID(nsIAbMDBCard), getter_AddRefs(dbCard));
            if(NS_SUCCEEDED(rv) && dbCard) {
                ConvertAssignPalmIDAttrib(aPalmRecordIDList[i], dbCard);
                nsCOMPtr<nsIAbCard> newCard;
                newCard = do_QueryInterface(dbCard, &rv);
                if(NS_SUCCEEDED(rv))
                    mABDB->EditCard(newCard, PR_FALSE);
            }
        }
    }

    if(mABDB && mDBOpen) {
        if(aSuccess) {
            rv = mABDB->Close(PR_TRUE);
            if(NS_SUCCEEDED(rv)) {
                mDBOpen = PR_FALSE;
                PRUint32 modTimeInSec;
                nsAddrDatabase::PRTime2Seconds(PR_Now(), &modTimeInSec);
                mDirServerInfo->PalmSyncTimeStamp = modTimeInSec;
                mDirServerInfo->PalmCategoryId = aPalmCatID;
                DIR_SavePrefsForOneServer(mDirServerInfo);
            }
        }
        if(NS_FAILED(rv) || !aSuccess) { // get back the previous file
            rv = mABDB->ForceClosed();
            if(NS_SUCCEEDED(rv)) {
                nsCAutoString leafName;
                mABFile->GetNativeLeafName(leafName);
                PRBool bExists=PR_FALSE;
                mPreviousABFile->Exists(&bExists);
                if(bExists) {
                    nsCOMPtr<nsIFile> parent;
                    rv = mABFile->GetParent(getter_AddRefs(parent));
                    if (NS_SUCCEEDED(rv)) {
                        mABFile->Remove(PR_FALSE);
                        mPreviousABFile->CopyToNative(parent, leafName);
                    }
                }
                mDBOpen = PR_FALSE;
            }
        }
    }
    
    return rv;
}
