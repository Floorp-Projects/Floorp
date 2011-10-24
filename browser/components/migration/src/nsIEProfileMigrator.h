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
#include <ole2.h>
#include "nsIBrowserProfileMigrator.h"
#include "nsIObserverService.h"
#include "nsTArray.h"
#include "nsINavHistoryService.h"

class nsIFile;
class nsICookieManager2;
class nsIRDFResource;
class nsINavBookmarksService;
class nsIPrefBranch;

struct SignonData {
  PRUnichar* user;
  PRUnichar* pass;
  char*      realm;
};

// VC11 doesn't ship with pstore.h, so we go ahead and define the stuff that
// we need from that file here.
class IEnumPStoreItems : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE Next(DWORD celt, LPWSTR* rgelt,
                                         DWORD* pceltFetched) = 0;
  virtual HRESULT STDMETHODCALLTYPE Skip(DWORD celt) = 0;
  virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
  virtual HRESULT STDMETHODCALLTYPE Clone(IEnumPStoreItems** ppenum) = 0;
};

class IEnumPStoreTypes; // not used
struct PST_PROVIDERINFO; // not used
struct PST_TYPEINFO; // not used
struct PST_PROMPTINFO; // not used
struct PST_ACCESSRULESET; // not used
typedef DWORD PST_KEY;
typedef DWORD PST_ACCESSMODE;

class IPStore : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE GetInfo(PST_PROVIDERINFO** ppProperties) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetProvParam(DWORD dwParam, DWORD* pcbData,
                                                 BYTE** ppbData, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetProvParam(DWORD dwParam, DWORD cbData,
                                                 BYTE* pbData, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateType(PST_KEY Key, const GUID* pType,
                                               PST_TYPEINFO* pInfo, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(PST_KEY Key, const GUID* pType,
                                                PST_TYPEINFO** ppInfo, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE DeleteType(PST_KEY Key, const GUID* pType,
                                               DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateSubtype(PST_KEY Key, const GUID* pType,
                                                  const GUID* pSubtype, PST_TYPEINFO* pInfo,
                                                  PST_ACCESSRULESET* pRules, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetSubtypeInfo(PST_KEY Key, const GUID* pType,
                                                   const GUID* pSubtype, PST_TYPEINFO** ppInfo,
                                                   DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE DeleteSubtype(PST_KEY Key, const GUID* pType,
                                                  const GUID* pSubtype, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReadAccessRuleset(PST_KEY Key, const GUID* pType,
                                                      const GUID* pSubtype, PST_ACCESSRULESET** ppRules,
                                                      DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE WriteAccessRuleset(PST_KEY Key, const GUID* pType,
                                                       const GUID* pSubtype, PST_ACCESSRULESET* pRules,
                                                       DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE EnumTypes(PST_KEY Key, DWORD dwFlags, IEnumPStoreTypes** ppenum) = 0;
  virtual HRESULT STDMETHODCALLTYPE EnumSubtypes(PST_KEY Key, const GUID* pType,
                                                 DWORD dwFlags, IEnumPStoreTypes** ppenum) = 0;
  virtual HRESULT STDMETHODCALLTYPE DeleteItem(PST_KEY Key, const GUID* pItemType,
                                               const GUID* pItemSubtype, LPCWSTR szItemName,
                                               PST_PROMPTINFO* pPromptInfo, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReadItem(PST_KEY Key, const GUID* pItemType,
                                             const GUID* pItemSubtype, LPCWSTR szItemName,
                                             DWORD* pcbData, BYTE** ppbData,
                                             PST_PROMPTINFO* pPromptInfo, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE WriteItem(PST_KEY Key, const GUID* pItemType,
                                              const GUID* pItemSubtype, LPCWSTR szItemName,
                                              DWORD cbData, BYTE* pbData,
                                              PST_PROMPTINFO* pPromptInfo, DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE OpenItem(PST_KEY Key, const GUID* pItemType,
                                             const GUID* pItemSubtype, LPCWSTR szItemName,
                                             PST_ACCESSMODE ModeFlags, PST_PROMPTINFO* pPromptInfo,
                                             DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE CloseItem(PST_KEY Key, const GUID* pItemType,
                                              const GUID* pItemSubtype, LPCWSTR szItemName,
                                              DWORD dwFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE EnumItems(PST_KEY Key, const GUID* pItemType,
                                              const GUID* pItemSubtype, DWORD dwFlags,
                                              IEnumPStoreItems** ppenum) = 0;
};


class nsIEProfileMigrator : public nsIBrowserProfileMigrator,
                            public nsINavHistoryBatchCallback {
public:
  NS_DECL_NSIBROWSERPROFILEMIGRATOR
  NS_DECL_NSINAVHISTORYBATCHCALLBACK
  NS_DECL_ISUPPORTS

  nsIEProfileMigrator();
  virtual ~nsIEProfileMigrator();

protected:
  nsresult CopyPreferences(bool aReplace);
  nsresult CopyStyleSheet(bool aReplace);
  nsresult CopyCookies(bool aReplace);
  nsresult CopyProxyPreferences(nsIPrefBranch* aPrefs);
  nsresult CopySecurityPrefs(nsIPrefBranch* aPrefs);
  /**
   * Migrate history to Places.
   * This will end up calling CopyHistoryBatched helper, that provides batch
   * support.  Batching allows for better performances and integrity.
   *
   * @param aReplace
   *        Indicates if we should replace current history or append to it.
   */
  nsresult CopyHistory(bool aReplace);
  nsresult CopyHistoryBatched(bool aReplace);

  bool     KeyIsURI(const nsAString& aKey, char** aRealm);

  nsresult CopyPasswords(bool aReplace);
  nsresult MigrateSiteAuthSignons(IPStore* aPStore);
  nsresult GetSignonsListFromPStore(IPStore* aPStore, nsTArray<SignonData>* aSignonsFound);
  nsresult ResolveAndMigrateSignons(IPStore* aPStore, nsTArray<SignonData>* aSignonsFound);
  void     EnumerateUsernames(const nsAString& aKey, PRUnichar* aData, unsigned long aCount, nsTArray<SignonData>* aSignonsFound);
  void     GetUserNameAndPass(unsigned char* data, unsigned long len, unsigned char** username, unsigned char** pass);

  nsresult CopyFormData(bool aReplace);
  nsresult AddDataToFormHistory(const nsAString& aKey, PRUnichar* data, unsigned long len);
  /**
   * Migrate bookmarks to Places.
   * This will end up calling CopyFavoritesBatched helper, that provides batch
   * support.  Batching allows for better performances and integrity.
   *
   * @param aReplace
   *        Indicates if we should replace current bookmarks or append to them.
   *        When appending we will usually default to bookmarks menu.
   */
  nsresult CopyFavorites(bool aReplace);
  nsresult CopyFavoritesBatched(bool aReplace);
  void     ResolveShortcut(const nsString &aFileName, char** aOutURL);
  nsresult ParseFavoritesFolder(nsIFile* aDirectory, 
                                PRInt64 aParentFolder,
                                nsINavBookmarksService* aBookmarksService,
                                const nsAString& aPersonalToolbarFolderName,
                                bool aIsAtRootLevel);
  nsresult CopySmartKeywords(nsINavBookmarksService* aBMS,
                             PRInt64 aParentFolder);

  nsresult CopyCookiesFromBuffer(char *aBuffer, PRUint32 aBufferLength,
                                 nsICookieManager2 *aCookieManager);
  void     DelimitField(char **aBuffer, const char *aBufferEnd, char **aField);
  time_t   FileTimeToTimeT(const char *aLowDateIntString,
                           const char *aHighDateIntString);
  void     GetUserStyleSheetFile(nsIFile **aUserFile);
  bool     TestForIE7();

private:
  nsCOMPtr<nsIObserverService> mObserverService;
};

#endif

