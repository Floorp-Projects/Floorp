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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#include "dom_util.h"

#include "ns_globals.h" // for prLogModuleInfo 

#include "prlog.h" // for PR_ASSERT

#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIDOMEventTarget.h"

nsresult dom_iterateToRoot(nsCOMPtr<nsIDOMNode> currentNode,
                           fpTakeActionOnNodeType nodeFunction, 
                           void *yourObject)
{
    nsCOMPtr<nsIDOMNode> parentNode;
    nsresult rv;

    if (nodeFunction) {
        rv = nodeFunction(currentNode, yourObject);
    }
    if (NS_FAILED(rv)) {
        return rv;
    }
    rv = currentNode->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(rv)) {
        return rv;
    }
    if (nsnull == parentNode) {
        return rv;
    }
    dom_iterateToRoot(parentNode, nodeFunction, yourObject);
    return rv;
}

nsCOMPtr<nsIDOMDocument> dom_getDocumentFromLoader(nsIDocumentLoader *loader)
{
    PR_ASSERT(nsnull != loader);
    nsresult rv;

    nsCOMPtr<nsISupports> container;
    nsCOMPtr<nsIDocShell> docShell;
    nsCOMPtr<nsIDOMDocument> result = nsnull;

    rv = loader->GetContainer(getter_AddRefs(container));
    if (NS_FAILED(rv) || !container) {
        return result;
    }

    rv = container->QueryInterface(NS_GET_IID(nsIDocShell), 
                                   getter_AddRefs(docShell));
    if (NS_FAILED(rv) || !docShell) {
        return result;
    }
    result = dom_getDocumentFromDocShell(docShell);

    return result;
}

nsCOMPtr<nsIDOMDocument> dom_getDocumentFromDocShell(nsIDocShell *docShell)
{
    PR_ASSERT(nsnull != docShell);
    nsCOMPtr<nsIDOMDocument> result = nsnull;
    nsCOMPtr<nsIContentViewer> contentv;
    nsCOMPtr<nsIDocumentViewer> docv;
    nsIDocument *document; // PENDING(edburns): does GetDocument do an AddRef?
    nsresult rv;

    rv = docShell->GetContentViewer(getter_AddRefs(contentv));
    if (NS_FAILED(rv) || !contentv) {
        return result;
    }

    rv = contentv->QueryInterface(NS_GET_IID(nsIDocumentViewer),
                                  getter_AddRefs(docv));
    if (NS_FAILED(rv) || !docv) {
        return result;
    }

    rv = docv->GetDocument(&document);
    if (NS_FAILED(rv) || !document) {
        return result;
    }
     
    rv = document->QueryInterface(NS_GET_IID(nsIDOMDocument), 
                                  getter_AddRefs(result));

    return result;
}
