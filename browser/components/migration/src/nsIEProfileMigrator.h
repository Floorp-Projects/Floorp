/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* Private header describing the class to migrate preferences from
   Windows Trident to Gecko. This is a virtual class. */

#ifndef ieprofilemigrator___h___
#define ieprofilemigrator___h___

#include <time.h>
#include <windows.h>
#include "nsIBrowserProfileMigrator.h"
#include "nsIObserverService.h"
#include "nsVoidArray.h"

class nsIFile;
class nsICookieManager2;
class nsIRDFResource;
class nsIBookmarksService;
class nsIPrefBranch;

#import PSTOREC_DLL raw_interfaces_only
using namespace PSTORECLib;

class nsIEProfileMigrator : public nsIBrowserProfileMigrator {
public:
  NS_DECL_NSIBROWSERPROFILEMIGRATOR
  NS_DECL_ISUPPORTS

  nsIEProfileMigrator();
  virtual ~nsIEProfileMigrator();

protected:
  nsresult CopyPreferences(PRBool aReplace);
  nsresult CopyStyleSheet(PRBool aReplace);
  nsresult CopyCookies(PRBool aReplace);
  nsresult CopyProxyPreferences(nsIPrefBranch* aPrefs);
  nsresult CopySecurityPrefs(nsIPrefBranch* aPrefs);
  nsresult CopyHistory(PRBool aReplace);

  PRBool   KeyIsURI(const nsAString& aKey, char** aRealm);

  nsresult CopyPasswords(PRBool aReplace);
  nsresult GetSignonsListFromPStore(IPStore* aPStore, nsVoidArray* aSignonsFound);
  nsresult ResolveAndMigrateSignons(IPStore* aPStore, nsVoidArray* aSignonsFound);
  void     EnumerateUsernames(const nsAString& aKey, PRUnichar* aData, unsigned long aCount, nsVoidArray* aSignonsFound);
  void     GetUserNameAndPass(unsigned char* data, unsigned long len, unsigned char** username, unsigned char** pass);

  nsresult CopyFormData(PRBool aReplace);
  nsresult AddDataToFormHistory(const nsAString& aKey, PRUnichar* data, unsigned long len);

  nsresult CopyFavorites(PRBool aReplace);
  void     ResolveShortcut(const nsAFlatString &aFileName, char** aOutURL);
  nsresult ParseFavoritesFolder(nsIFile* aDirectory, 
                                nsIRDFResource* aParentResource,
                                nsIBookmarksService* aBookmarksService, 
                                const nsAString& aPersonalToolbarFolderName,
                                PRBool aIsAtRootLevel);
  nsresult CopySmartKeywords(nsIRDFResource* aParentFolder);

  nsresult CopyCookiesFromBuffer(char *aBuffer, PRUint32 aBufferLength,
                                 nsICookieManager2 *aCookieManager);
  void     DelimitField(char **aBuffer, const char *aBufferEnd, char **aField);
  time_t   FileTimeToTimeT(const char *aLowDateIntString,
                           const char *aHighDateIntString);
  void     GetUserStyleSheetFile(nsIFile **aUserFile);

private:
  nsCOMPtr<nsIObserverService> mObserverService;
};

#endif

