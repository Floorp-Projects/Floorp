/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

class nsIRDFService;
class nsIRDFDataSource;
class nsIRDFResource;
class nsIRDFNode;
class nsICSSLoader;
class nsISimpleEnumerator;
class nsSupportsHashtable;
class nsIRDFContainer;
class nsIRDFContainerUtils;
class nsIDOMWindow;
class nsIDocument;

#include "nsIRDFCompositeDataSource.h"
#include "nsICSSStyleSheet.h"

class nsChromeRegistry : public nsIChromeRegistry
{
public:
  NS_DECL_ISUPPORTS

  // nsIChromeRegistry methods:
  NS_DECL_NSICHROMEREGISTRY

  // nsChromeRegistry methods:
  nsChromeRegistry();
  virtual ~nsChromeRegistry();

public:
  static nsresult FollowArc(nsIRDFDataSource *aDataSource,
                            nsCString& aResult, nsIRDFResource* aChromeResource,
                            nsIRDFResource* aProperty);

  static nsresult UpdateArc(nsIRDFDataSource *aDataSource, nsIRDFResource* aSource, nsIRDFResource* aProperty, 
                            nsIRDFNode *aTarget, PRBool aRemove);

protected:
  NS_IMETHOD GetDynamicDataSource(nsIURI *aChromeURL, PRBool aIsOverlay, PRBool aUseProfile, nsIRDFDataSource **aResult);
  NS_IMETHOD GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay, nsISimpleEnumerator **aResult);

  nsresult GetResource(const nsCString& aChromeType, nsIRDFResource** aResult);
  
  NS_IMETHOD UpdateDynamicDataSource(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, 
                                     PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove);
  NS_IMETHOD UpdateDynamicDataSources(nsIRDFDataSource *aDataSource, PRBool aIsOverlay, 
                                      PRBool aUseProfile, PRBool aRemove);
  NS_IMETHOD WriteInfoToDataSource(const char *aDocURI, const PRUnichar *aOverlayURI,
                                   PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove);
 
  nsresult LoadStyleSheet(nsICSSStyleSheet** aSheet, const nsCString & aURL);
  nsresult LoadStyleSheetWithURL(nsIURI* aURL, nsICSSStyleSheet** aSheet);
  
  nsresult GetUserSheetURL(PRBool aIsChrome, nsCString & aURL);

private:
  NS_IMETHOD LoadDataSource(const nsCString &aFileName, nsIRDFDataSource **aResult,
                            PRBool aUseProfileDirOnly = PR_FALSE, const char *aProfilePath = nsnull);

  NS_IMETHOD GetProfileRoot(nsCString& aFileURL);
  NS_IMETHOD GetInstallRoot(nsCString& aFileURL);

  NS_IMETHOD RefreshWindow(nsIDOMWindow* aWindow);

  NS_IMETHOD GetArcs(nsIRDFDataSource* aDataSource,
                        const nsCString& aType,
                        nsISimpleEnumerator** aResult);

  NS_IMETHOD AddToCompositeDataSource(PRBool aUseProfile);
  
  NS_IMETHOD GetBaseURL(const nsCString& aPackage, const nsCString& aProvider, 
                             nsCString& aBaseURL);

  NS_IMETHOD FindProvider(const nsCString& aPackage,
                          const nsCString& aProvider,
                          nsIRDFResource *aArc,
                          nsIRDFNode **aSelectedProvider);

  NS_IMETHOD SelectPackageInProvider(nsIRDFResource *aPackageList,
                                   const nsCString& aPackage,
                                   const nsCString& aProvider,
                                   const nsCString& aProviderName,
                                   nsIRDFResource *aArc,
                                   nsIRDFNode **aSelectedProvider);

  NS_IMETHOD SetProvider(const nsCString& aProvider,
                         nsIRDFResource* aSelectionArc,
                         const PRUnichar* aProviderName,
                         PRBool aAllUsers, 
                         const char *aProfilePath, 
                         PRBool aIsAdding);

  NS_IMETHOD SetProviderForPackage(const nsCString& aProvider,
                                   nsIRDFResource* aPackageResource, 
                                   nsIRDFResource* aProviderPackageResource, 
                                   nsIRDFResource* aSelectionArc, 
                                   PRBool aAllUsers, const char *aProfilePath, 
                                   PRBool aIsAdding);

  NS_IMETHOD SelectProviderForPackage(const nsCString& aProviderType,
                                        const PRUnichar *aProviderName, 
                                        const PRUnichar *aPackageName, 
                                        nsIRDFResource* aSelectionArc, 
                                        PRBool aUseProfile, PRBool aIsAdding);

  NS_IMETHOD InstallProvider(const nsCString& aProviderType,
                             const nsCString& aBaseURL,
                             PRBool aUseProfile, PRBool aAllowScripts, PRBool aRemove);

  nsresult ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength);

protected:
  PRBool mInstallInitialized;
  PRBool mProfileInitialized;
  nsCString mProfileRoot;
  nsCString mInstallRoot;

  nsCOMPtr<nsIRDFCompositeDataSource> mChromeDataSource;
  nsCOMPtr<nsIRDFDataSource> mUIDataSource;

  nsSupportsHashtable* mDataSourceTable;
  nsIRDFService* mRDFService;
  nsIRDFContainerUtils* mRDFContainerUtils;

  // Resources
  nsCOMPtr<nsIRDFResource> mSelectedSkin;
  nsCOMPtr<nsIRDFResource> mSelectedLocale;
  nsCOMPtr<nsIRDFResource> mBaseURL;
  nsCOMPtr<nsIRDFResource> mPackages;
  nsCOMPtr<nsIRDFResource> mPackage;
  nsCOMPtr<nsIRDFResource> mName;
  nsCOMPtr<nsIRDFResource> mLocType;
  nsCOMPtr<nsIRDFResource> mAllowScripts;

  // Style Sheets
  nsCOMPtr<nsICSSStyleSheet> mScrollbarSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserChromeSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserContentSheet;
};
