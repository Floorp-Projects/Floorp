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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef _nsContentSink_h_
#define _nsContentSink_h_

// Base class for contentsink implementations.

#include "nsICSSLoaderObserver.h"
#include "nsIScriptLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsAutoPtr.h"

class nsIDocument;
class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsICSSLoader;
class nsIParser;
class nsIAtom;
class nsIChannel;
class nsIContent;
class nsIViewManager;
class nsNodeInfoManager;

class nsContentSink : public nsICSSLoaderObserver,
                      public nsIScriptLoaderObserver,
                      public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER
  
  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify);

protected:
  nsContentSink();
  virtual ~nsContentSink();

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                nsISupports* aContainer, nsIChannel* aChannel);

  nsresult ProcessHTTPHeaders(nsIChannel* aChannel);
  nsresult ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                             nsIContent* aContent = nsnull);
  nsresult ProcessLinkHeader(nsIContent* aElement,
                             const nsAString& aLinkData);
  nsresult ProcessLink(nsIContent* aElement, const nsAString& aHref,
                       const nsAString& aRel, const nsAString& aTitle,
                       const nsAString& aType, const nsAString& aMedia);

  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsAString& aHref,
                                    PRBool aAlternate,
                                    const nsAString& aTitle,
                                    const nsAString& aType,
                                    const nsAString& aMedia);

  nsresult ProcessMETATag(nsIContent* aContent);

  void PrefetchHref(const nsAString &aHref, PRBool aExplicit);

  PRBool ScrollToRef(PRBool aReallyScroll);
  nsresult RefreshIfEnabled(nsIViewManager* vm);
  void StartLayout(PRBool aIsFrameset);

  // Overridable hooks into script evaluation
  virtual void PreEvaluateScript()  {return;}
  virtual void PostEvaluateScript() {return;}

  nsCOMPtr<nsIDocument>         mDocument;
  nsCOMPtr<nsIParser>           mParser;
  nsCOMPtr<nsIURI>              mDocumentURI;
  nsCOMPtr<nsIURI>              mDocumentBaseURI;
  nsCOMPtr<nsIDocShell>         mDocShell;
  nsCOMPtr<nsICSSLoader>        mCSSLoader;
  nsRefPtr<nsNodeInfoManager>   mNodeInfoManager;

  nsCOMArray<nsIScriptElement> mScriptElements;

  nsCString mRef; // ScrollTo #ref
  PRBool mNeedToBlockParser;
};

#endif // _nsContentSink_h_
