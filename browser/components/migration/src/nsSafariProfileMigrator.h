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
 *  Asaf Romano <mozilla.mano@sent.com>
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

#ifndef safariprofilemigrator___h___
#define safariprofilemigrator___h___

#include "nsIBrowserProfileMigrator.h"
#include "nsIObserverService.h"
#include "nsStringAPI.h"
#include "nsINavHistoryService.h"

#include <CoreFoundation/CoreFoundation.h>

class nsIRDFDataSource;

class nsSafariProfileMigrator : public nsIBrowserProfileMigrator,
                                public nsINavHistoryBatchCallback
{
public:
  NS_DECL_NSIBROWSERPROFILEMIGRATOR
  NS_DECL_NSINAVHISTORYBATCHCALLBACK
  NS_DECL_ISUPPORTS

  nsSafariProfileMigrator();
  virtual ~nsSafariProfileMigrator();

  typedef enum { STRING, INT, BOOL } PrefType;

  typedef nsresult(*prefConverter)(void*, nsIPrefBranch*);

  struct PrefTransform {
    CFStringRef   keyName;
    PrefType      type;
    const char*   targetPrefName;
    prefConverter prefSetterFunc;
    bool          prefHasValue;
    union {
      PRInt32     intValue;
      bool        boolValue;
      char*       stringValue;
    };
  };
  
  static nsresult SetBool(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetBoolInverted(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetString(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetInt(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetDefaultEncoding(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetDownloadFolder(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetDownloadHandlers(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetDownloadRetention(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetDisplayImages(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetFontName(void* aTransform, nsIPrefBranch* aBranch);
  static nsresult SetFontSize(void* aTransform, nsIPrefBranch* aBranch);
  static void CleanResource(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource);

protected:
  nsresult CopyPreferences(bool aReplace);
  nsresult CopyCookies(bool aReplace);
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
  /**
   * Migrate bookmarks to Places.
   * This will end up calling CopyBookmarksBatched helper, that provides batch
   * support.  Batching allows for better performances and integrity.
   *
   * @param aReplace
   *        Indicates if we should replace current bookmarks or append to them.
   *        When appending we will usually default to bookmarks menu.
   */
  nsresult CopyBookmarks(bool aReplace);
  nsresult CopyBookmarksBatched(bool aReplace);
  nsresult ParseBookmarksFolder(CFArrayRef aChildren, 
                                PRInt64 aParentFolder,
                                nsINavBookmarksService * aBookmarksService,
                                bool aIsAtRootLevel);
  nsresult CopyFormData(bool aReplace);
  nsresult CopyOtherData(bool aReplace);

  nsresult ProfileHasContentStyleSheet(bool *outExists);
  nsresult GetSafariUserStyleSheet(nsILocalFile** aResult);

private:
  bool HasFormDataToImport();
  nsCOMPtr<nsIObserverService> mObserverService;
};
 
#endif
