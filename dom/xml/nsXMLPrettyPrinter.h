/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLPrettyPrinter_h__
#define nsXMLPrettyPrinter_h__

#include "nsStubDocumentObserver.h"
#include "nsCOMPtr.h"

class nsIDocument;

class nsXMLPrettyPrinter : public nsStubDocumentObserver
{
public:
    nsXMLPrettyPrinter();

    NS_DECL_ISUPPORTS

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    /**
     * This will prettyprint the document if the document is loaded in a
     * displayed window.
     *
     * @param aDocument  document to prettyprint
     * @param [out] aDidPrettyPrint if true, and error not returned, actually
     *              went ahead with prettyprinting the document.
     */
    nsresult PrettyPrint(nsIDocument* aDocument, bool* aDidPrettyPrint);

    /**
     * Unhook the prettyprinter
     */
    void Unhook();
private:
    virtual ~nsXMLPrettyPrinter();

    /**
     * Signals for unhooking by setting mUnhookPending if the node changed is
     * not in the shadow root tree nor in anonymous content.
     *
     * @param aContent  content that has changed
     */
    void MaybeUnhook(nsIContent* aContent);

    nsIDocument* mDocument; //weak. Set as long as we're observing the document
    bool mUnhookPending;
};

nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter);

#endif //nsXMLPrettyPrinter_h__
