/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsStatusBarBiffManager.h"
#include "nsMsgBiffManager.h"
#include "nsCRT.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIMsgMailSession.h"
#include "nsMsgFolderDataSource.h"
#include "MailNewsTypes.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsISound.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kWindowMediatorCID,    NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,    NS_MSGMAILSESSION_CID);

// QueryInterface, AddRef, and Release
//
NS_IMPL_ISUPPORTS1(nsStatusBarBiffManager, nsIFolderListener)

nsIAtom * nsStatusBarBiffManager::kBiffStateAtom = nsnull;

nsStatusBarBiffManager::nsStatusBarBiffManager()
: mInitialized(PR_FALSE), mCurrentBiffState(nsIMsgFolder::nsMsgBiffState_NoMail)
{
	NS_INIT_ISUPPORTS();
}

nsStatusBarBiffManager::~nsStatusBarBiffManager()
{
    NS_RELEASE(kBiffStateAtom);
}

nsresult nsStatusBarBiffManager::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

	nsresult rv;

    kBiffStateAtom               = NS_NewAtom("BiffState");

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);

	mInitialized = PR_TRUE;
	return NS_OK;
}

nsresult nsStatusBarBiffManager::Shutdown()
{
	return NS_OK;
}

#define PREF_PLAY_SOUND_ON_NEW_MAIL      "mail.biff.play_sound"
#define PREF_PLAY_DEFAULT_SOUND          "mail.biff.use_default_sound"
#define PREF_USER_SPECIFIED_SOUND_FILE   "mail.biff.sound_file"
#define DEFAULT_NEW_MAIL_SOUND           "chrome://messenger/content/newmail.wav"

nsresult nsStatusBarBiffManager::PerformStatusBarBiff(PRUint32 newBiffFlag)
{
	// See nsMsgStatusFeedback
	nsresult rv;

    // if we got new mail, attempt to play a sound.
    // if we fail along the way, don't return.
    // we still need to update the UI.
    if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NewMail) {
      nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);
      if (pref) {
        PRBool playSoundOnBiff = PR_FALSE;
        rv = pref->GetBoolPref(PREF_PLAY_SOUND_ON_NEW_MAIL, &playSoundOnBiff);
        if (NS_SUCCEEDED(rv) && playSoundOnBiff) {
          nsCOMPtr<nsISound> sound = do_CreateInstance("@mozilla.org/sound;1");
          if (sound) {
            nsCOMPtr<nsIFileURL> soundURL = do_CreateInstance("@mozilla.org/network/standard-url;1");
            if (soundURL) {
              PRBool playDefaultSound = PR_TRUE;
              rv = pref->GetBoolPref(PREF_PLAY_DEFAULT_SOUND, &playDefaultSound);
              if (NS_SUCCEEDED(rv) && !playDefaultSound) {
                nsCOMPtr<nsILocalFile> soundFile;
                rv = pref->GetFileXPref(PREF_USER_SPECIFIED_SOUND_FILE, getter_AddRefs(soundFile));
                if (NS_SUCCEEDED(rv) && soundFile) {
                  nsCOMPtr <nsIFile> file = do_QueryInterface(soundFile);
                  if (file) {
                    rv = soundURL->SetFile(file);
                  }
                  else {
                    rv = NS_ERROR_FAILURE;
                  }
                }
              }

              if (NS_FAILED(rv) || playDefaultSound) {
                rv = soundURL->SetSpec(DEFAULT_NEW_MAIL_SOUND);
              }
            }

            if (NS_SUCCEEDED(rv) && soundURL) {
              sound->Play(soundURL);
            }
            else {
              sound->Beep();
            }
          } 
        }
      }
    }

	NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

	// why use DOM window enumerator instead of XUL window...????
	if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator))))
	{ 
		PRBool more;
	
		windowEnumerator->HasMoreElements(&more);

		while(more)
		{
			nsCOMPtr<nsISupports> nextWindow = nsnull;
			windowEnumerator->GetNext(getter_AddRefs(nextWindow));
			nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(nextWindow));
			NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

			nsCOMPtr<nsIDOMDocument> domDocument;
			domWindow->GetDocument(getter_AddRefs(domDocument));

			if(domDocument)
			{
				nsCOMPtr<nsIDOMElement> domElement;
				domDocument->GetElementById(NS_ConvertASCIItoUCS2("mini-mail"), getter_AddRefs(domElement));

				if (domElement) {
					if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NewMail) {
						domElement->SetAttribute(NS_ConvertASCIItoUCS2("BiffState"), NS_ConvertASCIItoUCS2("NewMail"));
					}
					else if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NoMail){
						domElement->RemoveAttribute(NS_ConvertASCIItoUCS2("BiffState"));
					}
				}
			}
			
			windowEnumerator->HasMoreElements(&more);
		}

	}

	return NS_OK;
}

// nsIFolderListener methods....
NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyChanged(nsISupports *item, nsIAtom *property, const char *oldValue, const char *newValue)
{
	return NS_OK;
}

NS_IMETHODIMP
nsStatusBarBiffManager::OnItemIntPropertyChanged(nsISupports *item, nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemBoolPropertyChanged(nsISupports *item, nsIAtom *property, PRBool oldValue, PRBool newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemUnicharPropertyChanged(nsISupports *item, nsIAtom *property, const PRUnichar *oldValue, const PRUnichar *newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
	if (kBiffStateAtom == property)
	{
		if (mCurrentBiffState != newFlag) {
			PerformStatusBarBiff(newFlag);
			mCurrentBiffState = newFlag;
		}
	}

	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemEvent(nsIFolder *item, nsIAtom *event)
{
	return NS_OK;
}
 
