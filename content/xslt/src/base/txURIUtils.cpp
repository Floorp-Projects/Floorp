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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *   Larry Fitzpatrick, OpenText <lef@opentext.com>
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

#include "txURIUtils.h"
#include "nsNetUtil.h"
#include "nsIAttribute.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsINodeInfo.h"

/**
 * URIUtils
 * A set of utilities for handling URIs
**/

/**
 * Resolves the given href argument, using the given documentBase
 * if necessary.
 * The new resolved href will be appended to the given dest String
**/
void URIUtils::resolveHref(const nsAString& href, const nsAString& base,
                           nsAString& dest) {
    if (base.IsEmpty()) {
        dest.Append(href);
        return;
    }
    if (href.IsEmpty()) {
        dest.Append(base);
        return;
    }
    nsCOMPtr<nsIURI> pURL;
    nsAutoString resultHref;
    nsresult result = NS_NewURI(getter_AddRefs(pURL), base);
    if (NS_SUCCEEDED(result)) {
        NS_MakeAbsoluteURI(resultHref, href, pURL);
        dest.Append(resultHref);
    }
} //-- resolveHref

// static
void
URIUtils::ResetWithSource(nsIDocument *aNewDoc, nsIDOMNode *aSourceNode)
{
    nsCOMPtr<nsINode> node = do_QueryInterface(aSourceNode);
    if (!node) {
        // XXXbz passing nsnull as the first arg to Reset is illegal
        aNewDoc->Reset(nsnull, nsnull);
        return;
    }

    nsCOMPtr<nsIDocument> sourceDoc = node->GetOwnerDoc();
    if (!sourceDoc) {
        NS_ERROR("no source document found");
        // XXXbz passing nsnull as the first arg to Reset is illegal
        aNewDoc->Reset(nsnull, nsnull);
        return;
    }

    nsIPrincipal* sourcePrincipal = sourceDoc->NodePrincipal();

    // Copy the channel and loadgroup from the source document.
    nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
    nsCOMPtr<nsIChannel> channel = sourceDoc->GetChannel();
    if (!channel) {
        // Need to synthesize one
        if (NS_FAILED(NS_NewChannel(getter_AddRefs(channel),
                                    sourceDoc->GetDocumentURI(),
                                    nsnull,
                                    loadGroup))) {
            return;
        }
        channel->SetOwner(sourcePrincipal);
    }
    aNewDoc->Reset(channel, loadGroup);
    aNewDoc->SetPrincipal(sourcePrincipal);
    aNewDoc->SetBaseURI(sourceDoc->GetDocBaseURI());

    // Copy charset
    aNewDoc->SetDocumentCharacterSetSource(
          sourceDoc->GetDocumentCharacterSetSource());
    aNewDoc->SetDocumentCharacterSet(sourceDoc->GetDocumentCharacterSet());
}
