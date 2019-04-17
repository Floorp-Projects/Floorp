/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutModule_h
#define nsLayoutModule_h

#include "nscore.h"
#include "nsID.h"
#include "mozilla/AlreadyAddRefed.h"

class nsIPresentationService;
class nsISupports;

// This function initializes various layout statics, as well as XPConnect.
// It should be called only once, and before the first time any XPCOM module in
// nsLayoutModule is used.
void nsLayoutModuleInitialize();

void nsLayoutModuleDtor();

nsresult CreateXMLContentSerializer(nsISupports* aOuter, const nsID& aIID,
                                    void** aResult);
nsresult CreateHTMLContentSerializer(nsISupports* aOuter, const nsID& aIID,
                                     void** aResult);
nsresult CreateXHTMLContentSerializer(nsISupports* aOuter, const nsID& aIID,
                                      void** aResult);
nsresult CreatePlainTextSerializer(nsISupports* aOuter, const nsID& aIID,
                                   void** aResult);
nsresult CreateContentPolicy(nsISupports* aOuter, const nsID& aIID,
                             void** aResult);
nsresult CreateGlobalMessageManager(nsISupports* aOuter, const nsID& aIID,
                                    void** aResult);
nsresult CreateParentMessageManager(nsISupports* aOuter, const nsID& aIID,
                                    void** aResult);
nsresult CreateChildMessageManager(nsISupports* aOuter, const nsID& aIID,
                                   void** aResult);

nsresult Construct_nsIScriptSecurityManager(nsISupports* aOuter,
                                            const nsIID& aIID, void** aResult);
nsresult LocalStorageManagerConstructor(nsISupports* aOuter, const nsIID& aIID,
                                        void** aResult);

already_AddRefed<nsIPresentationService> NS_CreatePresentationService();

#endif  // nsLayoutModule_h
