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

#ifndef nsRDFConMemberTestNode_h__
#define nsRDFConMemberTestNode_h__

#include "nscore.h"
#include "nsRDFTestNode.h"
#include "nsIRDFDataSource.h"
#include "nsFixedSizeAllocator.h"
class nsConflictSet;
class nsResourceSet;

/**
 * Rule network node that test if a resource is a member of an RDF
 * container, or is ``contained'' by another resource that refers to
 * it using a ``containment'' attribute.
 */
class nsRDFConMemberTestNode : public nsRDFTestNode
{
public:
    nsRDFConMemberTestNode(InnerNode* aParent,
                           nsConflictSet& aConflictSet,
                           nsIRDFDataSource* aDataSource,
                           const nsResourceSet& aMembershipProperties,
                           PRInt32 aContainerVariable,
                           PRInt32 aMemberVariable);

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const;

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool
    CanPropogate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const;

    virtual void
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget,
            nsTemplateMatchSet& aFirings,
            nsTemplateMatchSet& aRetractions) const;

    class Element : public MemoryElement {
    protected:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        Element(nsIRDFResource* aContainer,
                nsIRDFNode* aMember)
            : mContainer(aContainer),
              mMember(aMember) {
            MOZ_COUNT_CTOR(nsRDFConMemberTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFConMemberTestNode::Element); }

        static Element*
        Create(nsFixedSizeAllocator& aPool,
               nsIRDFResource* aContainer,
               nsIRDFNode* aMember) {
            void* place = aPool.Alloc(sizeof(Element));
            return place ? ::new (place) Element(aContainer, aMember) : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, Element* aElement) {
            aElement->~Element();
            aPool.Free(aElement, sizeof(*aElement)); }

        virtual const char* Type() const {
            return "nsRDFConMemberTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(NS_PTR_TO_INT32(mContainer.get())) ^
                (PLHashNumber(NS_PTR_TO_INT32(mMember.get())) >> 12); }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mContainer == element.mContainer && mMember == element.mMember;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone(void* aPool) const {
            return Create(*NS_STATIC_CAST(nsFixedSizeAllocator*, aPool),
                          mContainer, mMember); }

    protected:
        nsCOMPtr<nsIRDFResource> mContainer;
        nsCOMPtr<nsIRDFNode> mMember;
    };

protected:
    nsConflictSet& mConflictSet;
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    const nsResourceSet& mMembershipProperties;
    PRInt32 mContainerVariable;
    PRInt32 mMemberVariable;
};

#endif // nsRDFConMemberTestNode_h__
