/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *   -- 19990806
 *     -- moved initialization of constant shorts and chars from
 *        URIUtils.cpp to here
 *
 * Peter Van der Beken
 *
 */

#include "txURIUtils.h"

#ifndef TX_EXE
#include "nsNetUtil.h"
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
const char   URIUtils::DEVICE_SEP     = '|';
const char   URIUtils::PORT_SEP       = ':';
const char   URIUtils::PROTOCOL_SEP   = ':';


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

    istream* inStream = 0;

    ParsedURI* uri = parseURI(href);
    if ( !uri->isMalformed ) {
        inStream = openStream(uri);
    }
    else {
        // Try local files
        inStream = new ifstream(NS_LossyConvertUCS2toASCII(href).get(),
                                ios::in);
    }
    delete uri;

    return inStream;

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

    //-- check for URL
    ParsedURI* uri = parseURI(href);
    if ( !uri->isMalformed ) {
        dest.Append(href);
        delete uri;
        return;
    }


    //-- join document base + href
    nsAutoString xHref;
    if (!documentBase.IsEmpty()) {
        xHref.Append(documentBase);
        if (documentBase.CharAt(documentBase.Length()-1) != HREF_PATH_SEP)
            xHref.Append(PRUnichar(HREF_PATH_SEP));
    }
    xHref.Append(href);

    //-- check new href
    ParsedURI* newUri = parseURI(xHref);
    if ( !newUri->isMalformed ) {
        dest.Append(xHref);
    }
    else {
        // Try local files
        ifstream inFile(NS_LossyConvertUCS2toASCII(xHref).get(),
                        ios::in);
        if ( inFile ) dest.Append(xHref);
        else dest.Append(href);
        inFile.close();
    }
    delete uri;
    delete newUri;
#endif
} //-- resolveHref

#ifdef TX_EXE
istream* URIUtils::openStream(ParsedURI* uri) {
    if ( !uri ) return 0;
    // check protocol

    istream* inStream = 0;
    if (uri->protocol.Equals(NS_LITERAL_STRING("file"))) {
        ifstream* inFile =
            new ifstream(NS_LossyConvertUCS2toASCII(uri->path).get(),
                         ios::in);
        inStream = inFile;
    }

    return inStream;
} //-- openStream

URIUtils::ParsedURI* URIUtils::parseURI(const nsAString& aUri) {
    const nsAFlatString& uri = PromiseFlatString(aUri);
    ParsedURI* uriTokens = new ParsedURI;
    if (!uriTokens)
        return nsnull;
    uriTokens->isMalformed = MB_FALSE;

    ParseMode mode = PROTOCOL_MODE;

    // look for protocol
    int totalCount = uri.Length();
    int charCount = 0;
    PRUnichar prevCh = '\0';
    int fslash = 0;
    nsAutoString buffer;
    while ( charCount < totalCount ) {
        PRUnichar ch = uri.CharAt(charCount++);
        switch(ch) {
            case '.' :
                if ( mode == PROTOCOL_MODE ) {
                    uriTokens->isMalformed = MB_TRUE;
                    mode = HOST_MODE;
                }
                buffer.Append(ch);
                break;
            case ':' :
            {
                switch ( mode ) {
                    case PROTOCOL_MODE :
                        uriTokens->protocol = buffer;
                        buffer.Truncate();
                        mode = HOST_MODE;
                        break;
                    case HOST_MODE :
                        uriTokens->host = buffer;
                        buffer.Truncate();
                        mode = PORT_MODE;
                        break;
                    default:
                        break;
                }
                break;
            }
            case '/' :
                switch ( mode ) {
                    case HOST_MODE :
                        if (!buffer.IsEmpty()) {
                            mode = PATH_MODE;
                            buffer.Append(ch);
                        }
                        else if ( fslash == 2 ) mode = PATH_MODE;
                        else ++fslash;
                        break;
                    case PORT_MODE :
                        mode = PATH_MODE;
                        uriTokens->port.Append(buffer);
                        buffer.Truncate();
                        break;
                    default:
                        buffer.Append(ch);
                        break;
                }
                break;
            default:
                buffer.Append(ch);
        }
        prevCh = ch;
    }

    if ( mode == PROTOCOL_MODE ) {
        uriTokens->isMalformed = MB_TRUE;
    }
    //-- finish remaining mode
    if (!buffer.IsEmpty()) {
        switch ( mode ) {
            case PROTOCOL_MODE :
                uriTokens->protocol.Append(buffer);
                break;
            case HOST_MODE :
                uriTokens->host.Append(buffer);
                break;
            case PORT_MODE :
                uriTokens->port.Append(buffer);
                break;
            case PATH_MODE :
                uriTokens->path.Append(buffer);
                break;
            default:
                break;
        }
    }
    return uriTokens;
} //-- parseURI

#else /* TX_EXE */


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

    // Make sure that this is a real node. We do this by first QI'ing to
    // nsIContent (which is important performance wise) and if that QI
    // fails we QI to nsIDocument. If both those QI's fail we won't let
    // the caller access this unknown node.
    nsCOMPtr<nsIPrincipal> principal;
    nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

    if (!content) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aNode);

        if (!doc) {
            // aNode is neither a nsIContent nor an nsIDocument, something
            // weird is going on...

            NS_ERROR("aNode is neither an nsIContent nor an nsIDocument!");

            return PR_FALSE;
        }
        doc->GetPrincipal(getter_AddRefs(principal));
    }
    else {
        nsCOMPtr<nsIDOMDocument> domDoc;
        aNode->GetOwnerDocument(getter_AddRefs(domDoc));
        if (!domDoc) {
            nsCOMPtr<nsINodeInfo> ni;
            content->GetNodeInfo(*getter_AddRefs(ni));
            if (!ni) {
                // aNode is not part of a document, let any caller access it.

                return PR_TRUE;
            }

            ni->GetDocumentPrincipal(getter_AddRefs(principal));
        }
        else {
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
            NS_ASSERTION(doc, "QI to nsIDocument failed");
            doc->GetPrincipal(getter_AddRefs(principal));
        }
    }

    if (!principal) {
      // We can't get hold of the principal for this node. This should happen
      // very rarely, like for textnodes out of the tree and <option>s created
      // using 'new Option'.
      // If we didn't allow access to nodes like this you wouldn't be able to
      // insert these nodes into a document.

        return PR_TRUE;
    }

    nsresult rv = gTxSecurityManager->CheckSameOriginPrincipal(subjectPrincipal,
                                                               principal);

    return NS_SUCCEEDED(rv);
}


#endif /* TX_EXE */
