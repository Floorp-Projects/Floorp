/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Seth Spitzer <sspitzer@netscape.com>
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
#include "nsIFileChannel.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsIFile.h"

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
    NS_IF_RELEASE(kBiffStateAtom);
}

#define PREF_PLAY_SOUND_ON_NEW_MAIL      "mail.biff.play_sound"
#define PREF_NEW_MAIL_SOUND_URL          "mail.biff.play_sound.url"
#define PREF_NEW_MAIL_SOUND_TYPE         "mail.biff.play_sound.type"
#define SYSTEM_SOUND_TYPE 0
#define CUSTOM_SOUND_TYPE 1
#define DEFAULT_SYSTEM_SOUND "_moz_mailbeep"

nsresult nsStatusBarBiffManager::Init()
{
  if (mInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv;

  kBiffStateAtom = NS_NewAtom("BiffState");

  nsCOMPtr<nsIMsgMailSession> mailSession = 
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 
  if(NS_SUCCEEDED(rv))
    mailSession->AddFolderListener(this, nsIFolderListener::propertyFlagChanged);

  mInitialized = PR_TRUE;
  return NS_OK;
}

nsresult nsStatusBarBiffManager::PlayBiffSound()
{
  nsresult rv;
  nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  PRBool playSoundOnBiff = PR_FALSE;
  rv = pref->GetBoolPref(PREF_PLAY_SOUND_ON_NEW_MAIL, &playSoundOnBiff);
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (!playSoundOnBiff)
    return NS_OK;

  // lazily create the sound instance
  if (!mSound)
    mSound = do_CreateInstance("@mozilla.org/sound;1");
      
  PRInt32 newMailSoundType = SYSTEM_SOUND_TYPE;
  rv = pref->GetIntPref(PREF_NEW_MAIL_SOUND_TYPE, &newMailSoundType);
  NS_ENSURE_SUCCESS(rv,rv);

  PRBool customSoundPlayed = PR_FALSE;

  if (newMailSoundType == CUSTOM_SOUND_TYPE) {
    nsXPIDLCString soundURLSpec;
    rv = pref->GetCharPref(PREF_NEW_MAIL_SOUND_URL, getter_Copies(soundURLSpec));
    if (NS_SUCCEEDED(rv) && !soundURLSpec.IsEmpty()) {
      if (!strncmp(soundURLSpec.get(), "file://", 7)) {
        nsCOMPtr<nsIFileURL> soundURL = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
        
        rv = soundURL->SetSpec(soundURLSpec);                                       
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> soundFile;
          rv = soundURL->GetFile(getter_AddRefs(soundFile));
          if (NS_SUCCEEDED(rv)) {
            PRBool soundFileExists = PR_FALSE;
            rv = soundFile->Exists(&soundFileExists);
            if (NS_SUCCEEDED(rv) && soundFileExists) {
              rv = mSound->Play(soundURL);
              if (NS_SUCCEEDED(rv))
                customSoundPlayed = PR_TRUE;
            }
          }
        }
      }
      else {
        // todo, see if we can create a nsIFile using the string as a native path.
        // if that fails, try playing a system sound
        rv = mSound->PlaySystemSound(soundURLSpec.get());
        if (NS_SUCCEEDED(rv))
          customSoundPlayed = PR_TRUE;
      }
    }
  }    
  
  // if nothing played, play the default system sound
  if (!customSoundPlayed) {
    rv = mSound->PlaySystemSound(DEFAULT_SYSTEM_SOUND);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return rv;
}

nsresult nsStatusBarBiffManager::PerformStatusBarBiff(PRUint32 newBiffFlag)
{
  // See nsMsgStatusFeedback
  nsresult rv;
  
  // if we got new mail, attempt to play a sound.
  // if we fail along the way, don't return.
  // we still need to update the UI.    
  if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NewMail) {
    // if we fail to play the biff sound, keep going.
    (void)PlayBiffSound();
  }
  
  nsCOMPtr<nsIWindowMediator> windowMediator = 
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
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
        domDocument->GetElementById(NS_LITERAL_STRING("mini-mail"), getter_AddRefs(domElement));
        
        if (domElement) {
          if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NewMail) {
            domElement->SetAttribute(NS_LITERAL_STRING("BiffState"), NS_LITERAL_STRING("NewMail"));
          }
          else if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NoMail){
            domElement->RemoveAttribute(NS_LITERAL_STRING("BiffState"));
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
 
