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

/*

  A content model builder interface. An object that implements this
  interface is associated with an nsIXULDocument object to construct
  an NGLayout content model.

 */

#ifndef nsIRDFContentModelBuilder_h__
#define nsIRDFContentModelBuilder_h__

#include "nsISupports.h"

class nsIContent;

// {541AFCB0-A9A3-11d2-8EC5-00805F29F370}
#define NS_IRDFCONTENTMODELBUILDER_IID \
{ 0x541afcb0, 0xa9a3, 0x11d2, { 0x8e, 0xc5, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class nsIRDFContentModelBuilder : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFCONTENTMODELBUILDER_IID; return iid; }

    /**
     * Called to initialize a XUL content builder on a particular root
     * element. This element presumably has a ``datasources''
     * attribute, which the builder will parse to set up the template
     * builder's datasources.
     */
    NS_IMETHOD SetRootContent(nsIContent* aElement) = 0;

    /**
     * Invoked lazily by a XUL element that needs its child content
     * built.
     */
    NS_IMETHOD CreateContents(nsIContent* aElement) = 0;
};

extern NS_IMETHODIMP
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

extern NS_IMETHODIMP
NS_NewXULOutlinerBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif // nsIRDFContentModelBuilder_h__
