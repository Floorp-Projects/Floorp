/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): 
 */

#ifndef __searchOrphanImages_h__
#define __searchOrphanImages_h__

#include "inISearchOrphanImages.h"
#include "inISearchProcess.h"

#include "inISearchObserver.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsSupportsArray.h"
#include "nsHashtable.h"
#include "nsIDOMDocument.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsIFile.h"
#include "nsITimer.h"
#include "nsIRDFService.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContainer.h"

class inSearchOrphanImages : public inISearchOrphanImages,
                             public inISearchProcess
                             
{
public:
  inSearchOrphanImages();
  ~inSearchOrphanImages();

  nsresult SearchStep(PRBool* aDone);
  nsresult StopSearchTimer();

protected:
  nsIRDFService* mRDF;
  nsIRDFContainerUtils* mRDFCU;
  nsCOMPtr<nsITimer> mTimer;

  nsHashtable* mFileHash;
  nsCOMPtr<nsISupportsArray> mDirectories;
  
  PRUint32 mCurrentStep;
  PRUint32 mResultCount;
  nsCOMPtr<nsIRDFDataSource> mDataSource;
  nsCOMPtr<nsIRDFContainer> mResultSeq;
  nsCOMPtr<inISearchObserver> mObserver;
  
  nsAutoString mRemotePath;
  nsAutoString mSearchPath;
  PRBool mIsSkin;
  nsCOMPtr<nsIDOMDocument> mDocument;
  
  nsresult BuildRemoteURLHash();
  nsresult HashStyleSheet(nsIStyleSheet* aStyleSheet);
  nsresult HashStyleRule(nsIStyleRule* aStyleRule);
  nsresult HashStyleValue(nsICSSDeclaration* aDec, nsCSSProperty aProp);
  nsresult EqualizeRemoteURL(nsAutoString* aURL);
  nsAutoString EqualizeLocalURL(nsAutoString* aURL);
  nsresult SearchDirectory(nsIFile* aDir);
  nsAutoString GetFileExtension(nsIFile* aFile);
  nsresult StartSearchTimer();
  nsresult CacheAllDirectories();
  nsresult CacheDirectory(nsIFile* aDir);
  nsresult InitDataSource();
  nsresult CreateResourceFromFile(nsIFile* aFile, nsIRDFResource** aRes);
  nsresult ReportError(char* aMsg);

  static void SearchTimerCallback(nsITimer *aTimer, void *aClosure);

public:
  NS_DECL_ISUPPORTS

  NS_DECL_INISEARCHORPHANIMAGES

  NS_DECL_INISEARCHPROCESS

};

////////////////////////////////////////////////////////////////////

// {865761A8-7A39-426c-8114-396FA2B03768} 
#define IN_SEARCHORPHANIMAGES_CID \
{ 0x865761a8, 0x7a39, 0x426c, { 0x81, 0x14, 0x39, 0x6f, 0xa2, 0xb0, 0x37, 0x68 } }

#define IN_SEARCHORPHANIMAGES_CONTRACTID \
"@mozilla.org/inspector/search;1?name=findOrphanImages"

#endif
