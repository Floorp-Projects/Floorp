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
 *   Chris Waterson <waterson@netscape.com>
 */

#ifndef nsOutlinerRowTestNode_h__
#define nsOutlinerRowTestNode_h__

#include "nsFixedSizeAllocator.h"
#include "nsRuleNetwork.h"
#include "nsIRDFResource.h"
class nsConflictSet;
class nsOutlinerRows;

class nsOutlinerRowTestNode : public TestNode
{
public:
    enum { kRoot = -1 };

    nsOutlinerRowTestNode(InnerNode* aParent,
                          nsConflictSet& aConflictSet,
                          nsOutlinerRows& aRows,
                          PRInt32 aIdVariable);

    virtual nsresult
    FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const;

    virtual nsresult
    GetAncestorVariables(VariableSet& aVariables) const;

    class Element : public MemoryElement {
    public:
        static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
            return aAllocator.Alloc(aSize); }

        static void operator delete(void* aPtr, size_t aSize) {
            nsFixedSizeAllocator::Free(aPtr, aSize); }

        Element(nsIRDFResource* aResource)
            : mResource(aResource) {
            MOZ_COUNT_CTOR(nsOutlinerRowTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsOutlinerRowTestNode::Element); }

        virtual const char* Type() const {
            return "nsOutlinerRowTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(mResource.get()) >> 2; }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mResource == element.mResource;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone(void* aPool) const {
            return new (*NS_STATIC_CAST(nsFixedSizeAllocator*, aPool))
                Element(mResource); }

    protected:
        nsCOMPtr<nsIRDFResource> mResource;
    };

protected:
    nsConflictSet& mConflictSet;
    nsOutlinerRows& mRows;
    PRInt32 mIdVariable;
};

#endif // nsOutlinerRowTestNode_h__
