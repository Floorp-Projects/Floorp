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

#ifndef dom_util_h
#define dom_util_h

#include <jni.h> // for JNICALL

#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
class nsIDocumentLoader;
class nsIDocShell;

/**

 * Methods to simplify webclient accessing the mozilla DOM.

 */

/**

 * Used in conjunction with dom_iterateToRoot to allow action to be
 * taken on each node in a path from a node to the root.

 */

typedef nsresult (JNICALL *fpTakeActionOnNodeType) (nsCOMPtr<nsIDOMNode> curNode,
                                                    void *yourObject);

/**

 * Starting from startNode, recursively ascend the DOM tree to the root,
 * calling the user provided function at each node, passing the user
 * provided object to the function.

 */

nsresult dom_iterateToRoot(nsCOMPtr<nsIDOMNode> startNode,
                           fpTakeActionOnNodeType nodeFunction, 
                           void *yourObject);

/**

 * Given an nsIDocumentLoader instance, obtain the nsIDOMDocument instance.

 */ 

nsCOMPtr<nsIDOMDocument> dom_getDocumentFromLoader(nsIDocumentLoader *loader);

/**

 * Given an nsIDocShell instance, obtain the nsIDOMDocument instance.

 */ 

nsCOMPtr<nsIDOMDocument> dom_getDocumentFromDocShell(nsIDocShell *docShell);

#endif // dom_util_h
