/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (Initial Developer)
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

#ifndef nsChromeRegistryContent_h
#define nsChromeRegistryContent_h

#include "nsChromeRegistry.h"
#include "nsTArray.h"
#include "nsClassHashtable.h"

class nsCString;
struct ChromePackage;
struct ResourceMapping;
struct OverrideMapping;

class nsChromeRegistryContent : public nsChromeRegistry
{
 public:
  nsChromeRegistryContent();
  
  void RegisterRemoteChrome(const nsTArray<ChromePackage>& aPackages,
                            const nsTArray<ResourceMapping>& aResources,
                            const nsTArray<OverrideMapping>& aOverrides,
                            const nsACString& aLocale);

  NS_OVERRIDE NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                              nsIUTF8StringEnumerator* *aResult);
  NS_OVERRIDE NS_IMETHOD CheckForNewChrome();
  NS_OVERRIDE NS_IMETHOD CheckForOSAccessibility();
  NS_OVERRIDE NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                                 const PRUnichar* aData);
  NS_OVERRIDE NS_IMETHOD IsLocaleRTL(const nsACString& package,
                                     bool *aResult);
  NS_OVERRIDE NS_IMETHOD GetSelectedLocale(const nsACString& aPackage,
                                           nsACString& aLocale);
  NS_OVERRIDE NS_IMETHOD GetStyleOverlays(nsIURI *aChromeURL,
                                          nsISimpleEnumerator **aResult);
  NS_OVERRIDE NS_IMETHOD GetXULOverlays(nsIURI *aChromeURL,
                                        nsISimpleEnumerator **aResult);

 private:
  struct PackageEntry
  {
    PackageEntry() : flags(0) { }
    ~PackageEntry() { }

    nsCOMPtr<nsIURI> contentBaseURI;
    nsCOMPtr<nsIURI> localeBaseURI;
    nsCOMPtr<nsIURI> skinBaseURI;
    PRUint32         flags;
  };
  
  void RegisterPackage(const ChromePackage& aPackage);
  void RegisterResource(const ResourceMapping& aResource);
  void RegisterOverride(const OverrideMapping& aOverride);

  NS_OVERRIDE nsresult UpdateSelectedLocale();
  NS_OVERRIDE nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                 const nsCString& aProvider,
                                 const nsCString& aPath);
  NS_OVERRIDE nsresult GetFlagsFromPackage(const nsCString& aPackage, PRUint32* aFlags);

  nsClassHashtable<nsCStringHashKey, PackageEntry> mPackagesHash;
  nsCString mLocale;

  virtual void ManifestContent(ManifestProcessingContext& cx, int lineno,
                               char *const * argv, bool platform,
                               bool contentaccessible);
  virtual void ManifestLocale(ManifestProcessingContext& cx, int lineno,
                              char *const * argv, bool platform,
                              bool contentaccessible);
  virtual void ManifestSkin(ManifestProcessingContext& cx, int lineno,
                            char *const * argv, bool platform,
                            bool contentaccessible);
  virtual void ManifestOverlay(ManifestProcessingContext& cx, int lineno,
                               char *const * argv, bool platform,
                               bool contentaccessible);
  virtual void ManifestStyle(ManifestProcessingContext& cx, int lineno,
                             char *const * argv, bool platform,
                             bool contentaccessible);
  virtual void ManifestOverride(ManifestProcessingContext& cx, int lineno,
                                char *const * argv, bool platform,
                                bool contentaccessible);
  virtual void ManifestResource(ManifestProcessingContext& cx, int lineno,
                                char *const * argv, bool platform,
                                bool contentaccessible);
};

#endif // nsChromeRegistryContent_h
