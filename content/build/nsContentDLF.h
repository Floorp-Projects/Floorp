/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsContentDLF_h__
#define nsContentDLF_h__

#include "nsIDocumentLoaderFactory.h"
#include "nsIDocStreamLoaderFactory.h"
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

class nsContentDLF : public nsIDocumentLoaderFactory,
                     public nsIDocStreamLoaderFactory
{
public:
  nsContentDLF();
  virtual ~nsContentDLF();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCUMENTLOADERFACTORY

  // for nsIDocStreamLoaderFactory
  NS_METHOD CreateInstance(nsIInputStream& aInputStream,
                           const char* aContentType,
                           const char* aCommand,
                           nsISupports* aContainer,
                           nsISupports* aExtraInfo,
                           nsIContentViewer** aDocViewer);

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

  nsresult CreateXULDocumentFromStream(nsIInputStream& aXULStream,
                                       const char* aCommand,
                                       nsISupports* aContainer,
                                       nsISupports* aExtraInfo,
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
};

nsresult
NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);

#endif

