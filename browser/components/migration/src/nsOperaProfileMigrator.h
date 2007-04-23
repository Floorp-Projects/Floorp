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
 * The Original Code is The Browser Profile Migrator.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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
 
#ifndef operaprofilemigrator___h___
#define operaprofilemigrator___h___

#include "nsCOMPtr.h"
#include "nsIBinaryInputStream.h"
#include "nsIBrowserProfileMigrator.h"
#include "nsIObserverService.h"
#include "nsISupportsArray.h"
#include "nsStringAPI.h"
#include "nsVoidArray.h"

class nsICookieManager2;
class nsILineInputStream;
class nsILocalFile;
class nsINIParser;
class nsIPermissionManager;
class nsIPrefBranch;
#ifdef MOZ_PLACES_BOOKMARKS
class nsINavBookmarksService;
#else
class nsIBookmarksService;
#endif
class nsIRDFResource;

class nsOperaProfileMigrator : public nsIBrowserProfileMigrator
{
public:
  NS_DECL_NSIBROWSERPROFILEMIGRATOR
  NS_DECL_ISUPPORTS

  nsOperaProfileMigrator();
  virtual ~nsOperaProfileMigrator();

public:

  typedef enum { STRING, INT, BOOL, COLOR } PrefType;

  typedef nsresult(*prefConverter)(void*, nsIPrefBranch*);

  struct PrefTransform {
    char*         sectionName;
    char*         keyName;
    PrefType      type;
    char*         targetPrefName;
    prefConverter prefSetterFunc;
    PRBool        prefHasValue;
    union {
      PRInt32     intValue;
      PRBool      boolValue;
      char*       stringValue;
    };
  };

  static nsresult SetFile(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetCookieBehavior(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetImageBehavior(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetBool(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetWString(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetInt(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetString(void* aTransform, nsIPrefBranch* aBranch);

protected:
  nsresult CopyPreferences(PRBool aReplace);
  nsresult ParseColor(nsINIParser &aParser, const char* aSectionName, char** aResult);
  nsresult CopyUserContentSheet(nsINIParser &aParser);
  nsresult CopyProxySettings(nsINIParser &aParser, nsIPrefBranch* aBranch);
  nsresult GetInteger(nsINIParser &aParser, const char* aSectionName, 
                      const char* aKeyName, PRInt32* aResult);

  nsresult CopyCookies(PRBool aReplace);
  nsresult CopyHistory(PRBool aReplace);

  nsresult CopyBookmarks(PRBool aReplace);
#ifdef MOZ_PLACES_BOOKMARKS
  void     ClearToolbarFolder(nsINavBookmarksService * aBookmarksService, PRInt64 aToolbarFolder);
  nsresult ParseBookmarksFolder(nsILineInputStream* aStream, 
                                PRInt64 aFolder,
                                PRInt64 aToolbar, 
                                nsINavBookmarksService* aBMS);
#if defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
  nsresult CopySmartKeywords(nsINavBookmarksService* aBMS, 
                             nsIStringBundle* aBundle, 
                             PRInt64 aParentFolder);
#endif // defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
#else
  void     ClearToolbarFolder(nsIBookmarksService* aBookmarksService, nsIRDFResource* aToolbarFolder);
  nsresult ParseBookmarksFolder(nsILineInputStream* aStream, 
                                nsIRDFResource* aFolder,
                                nsIRDFResource* aToolbar, 
                                nsIBookmarksService* aBMS);
#if defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
  nsresult CopySmartKeywords(nsIBookmarksService* aBMS, 
                             nsIStringBundle* aBundle, 
                             nsIRDFResource* aParentFolder);
#endif // defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
#endif // MOZ_PLACES_BOOKMARKS

  void     GetOperaProfile(const PRUnichar* aProfile, nsILocalFile** aFile);

private:
  nsCOMPtr<nsILocalFile> mOperaProfile;
  nsCOMPtr<nsISupportsArray> mProfiles;
  nsCOMPtr<nsIObserverService> mObserverService;
};

class nsOperaCookieMigrator
{
public:
  nsOperaCookieMigrator(nsIInputStream* aSourceStream);
  virtual ~nsOperaCookieMigrator();

  nsresult Migrate();

  typedef enum { BEGIN_DOMAIN_SEGMENT         = 0x01,
                 DOMAIN_COMPONENT             = 0x1E,
                 END_DOMAIN_SEGMENT           = 0x84 | 0x80, // 0x04 | (1 << 8)
                 
                 BEGIN_PATH_SEGMENT           = 0x02,
                 PATH_COMPONENT               = 0x1D,
                 END_PATH_SEGMENT             = 0x05 | 0x80, // 0x05 | (1 << 8)
                 
                 FILTERING_INFO               = 0x1F,
                 PATH_HANDLING_INFO           = 0x21,
                 THIRD_PARTY_HANDLING_INFO    = 0x25,

                 BEGIN_COOKIE_SEGMENT         = 0x03,
                 COOKIE_ID                    = 0x10,
                 COOKIE_DATA                  = 0x11,
                 COOKIE_EXPIRY                = 0x12,
                 COOKIE_LASTUSED              = 0x13,
                 COOKIE_COMMENT               = 0x14,
                 COOKIE_COMMENT_URL           = 0x15,
                 COOKIE_V1_DOMAIN             = 0x16,
                 COOKIE_V1_PATH               = 0x17,
                 COOKIE_V1_PORT_LIMITATIONS   = 0x18,
                 COOKIE_SECURE                = 0x19 | 0x80, 
                 COOKIE_VERSION               = 0x1A,
                 COOKIE_OTHERFLAG_1           = 0x1B | 0x80,
                 COOKIE_OTHERFLAG_2           = 0x1C | 0x80,
                 COOKIE_OTHERFLAG_3           = 0x20 | 0x80,
                 COOKIE_OTHERFLAG_4           = 0x22 | 0x80,
                 COOKIE_OTHERFLAG_5           = 0x23 | 0x80,
                 COOKIE_OTHERFLAG_6           = 0x24 | 0x80
  } TAG;

protected:
  nsOperaCookieMigrator() { }

  nsresult ReadHeader();

  void     SynthesizePath(char** aResult);
  void     SynthesizeDomain(char** aResult);
  nsresult AddCookieOverride(nsIPermissionManager* aManager);
  nsresult AddCookie(nsICookieManager2* aManager);

private:
  nsCOMPtr<nsIBinaryInputStream> mStream;

  nsVoidArray mDomainStack;
  nsVoidArray mPathStack;

  struct Cookie {
    nsCString id;
    nsCString data;
    PRInt32 expiryTime;
    PRBool isSecure;
  };

  PRUint32 mAppVersion;
  PRUint32 mFileVersion;
  PRUint16 mTagTypeLength;
  PRUint16 mPayloadTypeLength;
  PRBool   mCookieOpen;
  Cookie   mCurrCookie;
  PRUint8  mCurrHandlingInfo;

};

#endif

