/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#ifndef TRANSFRMX_NS_NODESET_H
#define TRANSFRMX_NS_NODESET_H

#include "NodeSet.h"
#include "nsIDOMNodeList.h"
#include "nsSupportsArray.h"

class nsNodeSet : public nsIDOMNodeList
{
public:
    /**
     * Creates a new Mozilla NodeSet from a Transformiix NodeSet
    **/
    nsNodeSet(NodeSet* aNodeSet);

    /**
     * Default destructor for nsNodeSet
    **/
    virtual ~nsNodeSet();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDocumentTransformer interface
    NS_DECL_NSIDOMNODELIST

private:
    void* mScriptObject;
    nsSupportsArray mNodes;
};

#endif
