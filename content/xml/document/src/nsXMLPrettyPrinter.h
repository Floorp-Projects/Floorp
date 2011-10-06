/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original author)
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
 
#ifndef nsXMLPrettyPrinter_h__
#define nsXMLPrettyPrinter_h__

#include "nsStubDocumentObserver.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"

class nsXMLPrettyPrinter : public nsStubDocumentObserver
{
public:
    nsXMLPrettyPrinter();
    virtual ~nsXMLPrettyPrinter();

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
    /**
     * Signals for unhooking by setting mUnhookPending if the node changed is
     * non-anonymous content.
     *
     * @param aContent  content that has changed
     */
    void MaybeUnhook(nsIContent* aContent);

    nsIDocument* mDocument; //weak. Set as long as we're observing the document
    PRUint32 mUpdateDepth;
    bool mUnhookPending;
};

nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter);

#endif //nsXMLPrettyPrinter_h__
