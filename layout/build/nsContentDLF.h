/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsContentDLF_h__
#define nsContentDLF_h__

#include "nsIDocumentLoaderFactory.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"

class nsICSSStyleSheet;
class nsIChannel;
class nsIComponentManager;
class nsIContentViewer;
class nsIDocumentViewer;
class nsIFile;
class nsIInputStream;
class nsILoadGroup;
class nsIStreamListener;
struct nsModuleComponentInfo;

class nsContentDLF : public nsIDocumentLoaderFactory
{
public:
  nsContentDLF();
  virtual ~nsContentDLF();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCUMENTLOADERFACTORY

  nsresult InitUAStyleSheet();

  nsresult CreateDocument(const char* aCommand,
                          nsIChannel* aChannel,
                          nsILoadGroup* aLoadGroup,
                          nsISupports* aContainer,
                          const nsCID& aDocumentCID,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer);

  nsresult CreateRDFDocument(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             const char* aContentType,
                             nsISupports* aContainer,
                             nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer);

  nsresult CreateRDFDocument(nsISupports*,
                             nsCOMPtr<nsIDocument>*,
                             nsCOMPtr<nsIDocumentViewer>*);

  static nsICSSStyleSheet* gUAStyleSheet;

  static NS_IMETHODIMP
  RegisterDocumentFactories(nsIComponentManager* aCompMgr,
                            nsIFile* aPath,
                            const char *aLocation,
                            const char *aType,
                            const nsModuleComponentInfo* aInfo);

  static NS_IMETHODIMP
  UnregisterDocumentFactories(nsIComponentManager* aCompMgr,
                              nsIFile* aPath,
                              const char* aRegistryLocation,
                              const nsModuleComponentInfo* aInfo);

private:
  static nsresult EnsureUAStyleSheet();
};

nsresult
NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);

#endif

