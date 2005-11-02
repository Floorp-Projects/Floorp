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

#ifndef TX_EXE
#include "nsNetUtil.h"
#include "nsIAttribute.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsINodeInfo.h"
#endif

/**
 * URIUtils
 * A set of utilities for handling URIs
**/

#ifdef TX_EXE
//- Constants -/

const char   URIUtils::HREF_PATH_SEP  = '/';

/**
 * Implementation of utility functions for parsing URLs.
 * Just file paths for now.
 */
void
txParsedURL::init(const nsAFlatString& aSpec)
{
    mPath.Truncate();
    mName.Truncate();
    mRef.Truncate();
    PRUint32 specLength = aSpec.Length();
    if (!specLength) {
        return;
    }
    const PRUnichar* start = aSpec.get();
    const PRUnichar* end = start + specLength;
    const PRUnichar* c = end - 1;

    // check for #ref
    for (; c >= start; --c) {
        if (*c == '#') {
            // we could eventually unescape this, too.
            mRef = Substring(c + 1, end);
            end = c;
            --c;
            if (c == start) {
                // we're done,
                return;
            }
            break;
        }
    }
    for (c = end - 1; c >= start; --c) {
        if (*c == '/') {
            mName = Substring(c + 1, end);
            mPath = Substring(start, c + 1);
            return;
        }
    }
    mName = Substring(start, end);
}

void
txParsedURL::resolve(const txParsedURL& aRef, txParsedURL& aDest)
{
    /*
     * No handling of absolute URLs now.
     * These aren't really URLs yet, anyway, but paths with refs
     */
    aDest.mPath = mPath + aRef.mPath;

    if (aRef.mName.IsEmpty() && aRef.mPath.IsEmpty()) {
        // the relative URL is just a fragment identifier
        aDest.mName = mName;
        if (aRef.mRef.IsEmpty()) {
            // and not even that, keep the base ref
            aDest.mRef = mRef;
            return;
        }
        aDest.mRef = aRef.mRef;
        return;
    }
    aDest.mName = aRef.mName;
    aDest.mRef = aRef.mRef;
}

/**
 * Returns an InputStream for the file represented by the href
 * argument
 * @param href the href of the file to get the input stream for.
 * @return an InputStream to the desired resource
 * @exception java.io.FileNotFoundException when the file could not be
 * found
**/
istream* URIUtils::getInputStream(const nsAString& href, nsAString& errMsg)
{
    return new ifstream(NS_LossyConvertUCS2toASCII(href).get(), ios::in);
} //-- getInputStream

/**
    * Returns the document base of the href argument
    * @return the document base of the given href
**/
void URIUtils::getDocumentBase(const nsAFlatString& href, nsAString& dest)
{
    if (href.IsEmpty()) {
        return;
    }

    nsAFlatString::const_char_iterator temp;
    href.BeginReading(temp);
    PRUint32 iter = href.Length();
    while (iter > 0) {
        if (temp[--iter] == HREF_PATH_SEP) {
            dest.Append(Substring(href, 0, iter));
            break;
        }
    }
}
#endif

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

#ifndef TX_EXE
    nsCOMPtr<nsIURI> pURL;
    nsAutoString resultHref;
    nsresult result = NS_NewURI(getter_AddRefs(pURL), base);
    if (NS_SUCCEEDED(result)) {
        NS_MakeAbsoluteURI(resultHref, href, pURL);
        dest.Append(resultHref);
    }
#else
    nsAutoString documentBase;
    getDocumentBase(PromiseFlatString(base), documentBase);

    //-- join document base + href
    if (!documentBase.IsEmpty()) {
        dest.Append(documentBase);
        if (documentBase.CharAt(documentBase.Length()-1) != HREF_PATH_SEP)
            dest.Append(PRUnichar(HREF_PATH_SEP));
    }
    dest.Append(href);

#endif
} //-- resolveHref

#ifndef TX_EXE

nsIScriptSecurityManager *gTxSecurityManager = 0;

// static
PRBool URIUtils::CanCallerAccess(nsIDOMNode *aNode)
{
    if (!gTxSecurityManager) {
        // No security manager available, let any calls go through...

        return PR_TRUE;
    }

    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    gTxSecurityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

    if (!subjectPrincipal) {
        // we're running as system, grant access to the node.

        return PR_TRUE;
    }

    // Check whether the subject principal is the system principal.
    // For performance, we will avoid calling SubjectPrincipalIsChrome()
    // since it calls GetSubjectPrincipal() which causes us to walk
    // the JS frame stack.  We already did that above, so just get the
    // system principal from the security manager, and do a raw comparison.
    nsCOMPtr<nsIPrincipal> systemPrincipal;
    gTxSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));

    if (subjectPrincipal == systemPrincipal) {
        // we're running as system, grant access to the node.

        return PR_TRUE;
    }

    // Ask the securitymanager if we have "UniversalBrowserRead"
    PRBool caps = PR_FALSE;
    nsresult rv =
        gTxSecurityManager->IsCapabilityEnabled("UniversalBrowserRead",
                                                &caps);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    if (caps) {
        return PR_TRUE;
    }

    // Make sure that this is a real node. We do this by first QI'ing to
    // nsIContent (which is important performance wise) and if that QI
    // fails we QI to nsIDocument. If both those QI's fail we won't let
    // the caller access this unknown node.
    nsIPrincipal *principal = nsnull;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    nsCOMPtr<nsIAttribute> attr;
    nsCOMPtr<nsIDocument> doc;

    if (!content) {
        doc = do_QueryInterface(aNode);

        if (!doc) {
            attr = do_QueryInterface(aNode);
            if (!attr) {
                // aNode is not a nsIContent, a nsIAttribute or a nsIDocument,
                // something weird is going on...

                NS_ERROR("aNode is not a nsIContent, a nsIAttribute or a nsIDocument!");

                return PR_FALSE;
            }
        }
    }

    if (!doc) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        aNode->GetOwnerDocument(getter_AddRefs(domDoc));
        if (!domDoc) {
            nsINodeInfo *ni;
            if (content) {
                ni = content->GetNodeInfo();
            }
            else {
                ni = attr->NodeInfo();
            }

            if (!ni) {
                // aNode is not part of a document, let any caller access it.

                return PR_TRUE;
            }

            principal = ni->GetDocumentPrincipal();
            if (!principal) {
              // we can't get to the principal so we'll give up and give the
              // caller access

              return PR_TRUE;
            }
        }
        else {
            doc = do_QueryInterface(domDoc);
            NS_ASSERTION(doc, "QI to nsIDocument failed");
        }
    }

    if (!principal) {
        principal = doc->GetPrincipal();
    }

    if (!principal) {
        // We can't get hold of the principal for this node. This should happen
        // very rarely, like for textnodes out of the tree and <option>s created
        // using 'new Option'.

        return PR_TRUE;
    }

    rv = gTxSecurityManager->CheckSameOriginPrincipal(subjectPrincipal,
                                                      principal);

    return NS_SUCCEEDED(rv);
}

// static
void
URIUtils::ResetWithSource(nsIDocument *aNewDoc, nsIDOMNode *aSourceNode)
{
    if (!aSourceNode) {
        aNewDoc->Reset(nsnull, nsnull);
        return;
    }

    nsCOMPtr<nsIDocument> sourceDoc = do_QueryInterface(aSourceNode);
    if (!sourceDoc) {
        nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
        aSourceNode->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
        sourceDoc = do_QueryInterface(sourceDOMDocument);
    }
    if (!sourceDoc) {
        NS_ASSERTION(0, "no source document found");
        aNewDoc->Reset(nsnull, nsnull);
        return;
    }

    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (serv) {
        // Create a temporary channel to get nsIDocument->Reset to
        // do the right thing. We want the output document to get
        // much of the input document's characteristics.
        serv->NewChannelFromURI(sourceDoc->GetDocumentURI(),
                                getter_AddRefs(channel));
    }
    aNewDoc->Reset(channel, loadGroup);
    aNewDoc->SetBaseURI(sourceDoc->GetBaseURI());

    // Copy charset
    aNewDoc->SetDocumentCharacterSet(sourceDoc->GetDocumentCharacterSet());
    aNewDoc->SetDocumentCharacterSetSource(
          sourceDoc->GetDocumentCharacterSetSource());
}

#endif /* TX_EXE */
