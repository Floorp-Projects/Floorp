/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Seth Spitzer <sspitzer@netscape.com>
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

#include "nscore.h"
#include "nsIMessengerMigrator.h"
#include "nsCOMPtr.h"
#include "nsISmtpServer.h"
#include "nsIPref.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgIncomingServer.h"
#include "nsIObserver.h"

/*
 * some platforms (like Windows and Mac) use a map file, because of
 * file name length limitations.
 */
#if defined(XP_UNIX) || defined(XP_BEOS)
/* in 4.x, the prefix was ".newsrc-" and ".snewsrc-"
 * in 5.0, the profile migrator code copies the newsrc files from
 * ~/.newsrc-* to ~/.mozilla/<profile>/News/newsrc-*
 * ~/.snewsrc-* to ~/.mozilla/<profile>/News/snewsrc-*
 */
#define NEWSRC_FILE_PREFIX_IN_5x "newsrc-"
#define SNEWSRC_FILE_PREFIX_IN_5x "snewsrc-"
#else
#define USE_NEWSRC_MAP_FILE

// in the fat file, the hostname is prefix by one of these:
#define PSUEDO_NAME_PREFIX "newsrc-"
#define PSUEDO_SECURE_NAME_PREFIX "snewsrc-"

#if defined(XP_PC)
/* another case of mac vs. windows in 4.x
 * on mac, the default for news fcc, if you used imap, 
 * was "Sent on Local Mail"
 * on windows, the default for news fcc, 
 * if you used imap, was "Sent on <imap server>"
 */
#define NEWS_FCC_DEFAULT_TO_IMAP_SENT
#define NEWS_FAT_FILE_NAME "fat"
/*
 * on the PC, the fat file stores absolute paths to the newsrc files
 * on the Mac, the fat file stores relative paths to the newsrc files
 */
#define NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS 1
#elif defined(XP_MAC)
#define NEWS_FAT_FILE_NAME "NewsFAT"
#else
#error dont_know_what_your_news_fat_file_is
#endif /* XP_PC, XP_MAC */

#endif /* XP_UNIX || XP_BEOS */

class nsMessengerMigrator
	: public nsIMessengerMigrator, public nsIObserver
{
public:

  nsMessengerMigrator();
  virtual ~nsMessengerMigrator();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMESSENGERMIGRATOR
  NS_DECL_NSIOBSERVER  

  nsresult Init();
  nsresult Shutdown();

private:

  nsresult MigrateIdentity(nsIMsgIdentity *identity);
  nsresult MigrateSmtpServer(nsISmtpServer *server);
  nsresult SetMailCopiesAndFolders(nsIMsgIdentity *identity, const char *username, const char *hostname);
  nsresult SetNewsCopiesAndFolders(nsIMsgIdentity *identity);
  nsresult SetUsernameIfNecessary();

  nsresult MigrateImapAccounts(nsIMsgIdentity *identity);
  nsresult MigrateImapAccount(nsIMsgIdentity *identity, const char *hostAndPort, PRBool isDefaultAccount);
  
  nsresult MigrateOldImapPrefs(nsIMsgIncomingServer *server, const char *hostAndPort);
  
  nsresult MigratePopAccount(nsIMsgIdentity *identity);

#ifdef HAVE_MOVEMAIL
  nsresult MigrateMovemailAccount(nsIMsgIdentity *identity);
#endif /* HAVE_MOVEMAIL */
  
  nsresult MigrateLocalMailAccount();

  nsresult MigrateOldMailPrefs(nsIMsgIncomingServer *server);
  
  nsresult MigrateNewsAccounts(nsIMsgIdentity *identity);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostAndPort, nsFileSpec &newsrcfile, nsFileSpec &newsHostsDir, PRBool isSecure);
  nsresult MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostAndPort, nsFileSpec &newsrcfile);

  nsresult MigrateAddressBooks();
  static void migrateAddressBookPrefEnum(const char *aPref, void *aClosure);
        
  nsresult ProceedWithMigration();
  
  nsresult Convert4XUri(const char *old_uri, PRBool for_news, const char *aUsername, const char *aHostname, const char *default_folder_name, const char *default_pref_name, char **new_uri);
 
  nsresult SetSendLaterUriPref(nsIMsgIncomingServer *server);

  nsresult getPrefService();
  nsresult initializeStrings();

  nsCOMPtr <nsIPref> m_prefs;
  PRBool m_haveShutdown;
  PRInt32 m_oldMailType;
  PRBool m_alreadySetNntpDefaultLocalPath;
  PRBool m_alreadySetImapDefaultLocalPath;

  nsCString mLocalFoldersHostname;
  nsString mLocalFoldersName;
    
};

