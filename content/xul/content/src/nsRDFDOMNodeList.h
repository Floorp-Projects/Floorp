/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsRDFDOMNodeList_h__
#define nsRDFDOMNodeList_h__

#include "nsIDOMNodeList.h"
#include "nsIRDFNodeList.h"
class nsIDOMNode;
class nsISupportsArray;

class nsRDFDOMNodeList : public nsIDOMNodeList,
                         public nsIRDFNodeList
{
private:
    //nsISupports* mInner; Not being used?
    nsISupportsArray* mElements;

    nsRDFDOMNodeList(void);
    nsresult Init(void);

public:
    static nsresult Create(nsRDFDOMNodeList** aResult);
    static nsresult CreateWithArray(nsISupportsArray* aArray, nsRDFDOMNodeList** aResult);

    virtual ~nsRDFDOMNodeList(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNodeList interface
    NS_DECL_NSIDOMNODELIST

    // Implementation methods
    NS_IMETHOD AppendNode(nsIDOMNode* aNode);
    NS_IMETHOD RemoveNode(nsIDOMNode* aNode);
};

#endif // nsRDFDOMNodeList_h__

