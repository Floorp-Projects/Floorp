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

  Private interface to XUL content.

*/

#ifndef nsIXULContent_h__
#define nsIXULContent_h__

#include "nsISupports.h"
class nsIAtom;
class nsIRDFCompositeDataSource;
class nsIXULTemplateBuilder;
class nsString;

// {39C5ECC0-5C47-11d3-BE36-00104BDE6048}
#define NS_IXULCONTENT_IID \
{ 0x39c5ecc0, 0x5c47, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


class nsIXULContent : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULCONTENT_IID; return iid; }

    /**
     * Peek at a XUL element's child count without forcing children to be
     * instantiated.
     */
    NS_IMETHOD PeekChildCount(PRInt32& aCount) const = 0;

    /**
     * These flags are used to maintain bookkeeping information for partially-
     * constructed content.
     *
     *   eChildrenMustBeRebuilt
     *     The element's children are invalid or unconstructed, and should
     *     be reconstructed.
     *
     *   eTemplateContentsBuilt
     *     Child content that is built from a XUL template has been
     *     constructed. 
     *
     *   eContainerContentsBuilt
     *     Child content that is built by following the ``containment''
     *     property in a XUL template has been built.
     */
    enum {
        eChildrenMustBeRebuilt  = 0x1,
        eTemplateContentsBuilt  = 0x2,
        eContainerContentsBuilt = 0x4
    };

    /**
     * Set one or more ``lazy state'' flags.
     * @aFlags a mask of flags to set
     */
    NS_IMETHOD SetLazyState(PRInt32 aFlags) = 0;

    /**
     * Clear one or more ``lazy state'' flags.
     * @aFlags a mask of flags to clear
     */
    NS_IMETHOD ClearLazyState(PRInt32 aFlags) = 0;

    /**
     * Get the value of a single ``lazy state'' flag.
     * @aFlag a flag to test
     * @aResult the result
     */
    NS_IMETHOD GetLazyState(PRInt32 aFlag, PRBool& aResult) = 0;

    /**
     * Add a script event listener to the element.
     */
    NS_IMETHOD AddScriptEventListener(nsIAtom* aName, const nsAReadableString& aValue, REFNSIID aIID) = 0;

    /**
     * Evil rotten hack to make mailnews work. They assume that we
     * keep a reference to their resource objects. If you think you
     * should call this method, think again. You shouldn't.
     */
    NS_IMETHOD ForceElementToOwnResource(PRBool aForce) = 0;

    /**
     * Initialize the root element in a XUL template
     */
    NS_IMETHOD InitTemplateRoot(nsIRDFCompositeDataSource* aDatabase,
                                nsIXULTemplateBuilder* aBuilder) = 0;
};

#endif // nsIXULContent_h__
