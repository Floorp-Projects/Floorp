/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFConInstanceTestNode_h__
#define nsRDFConInstanceTestNode_h__

#include "nscore.h"
#include "nsFixedSizeAllocator.h"
#include "nsRDFTestNode.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsXULTemplateQueryProcessorRDF.h"

/**
 * Rule network node that tests if a resource is an RDF container, or
 * uses multi-attributes to ``contain'' other elements.
 */
class nsRDFConInstanceTestNode : public nsRDFTestNode
{
public:
    enum Test { eFalse, eTrue, eDontCare };

    nsRDFConInstanceTestNode(TestNode* aParent,
                             nsXULTemplateQueryProcessorRDF* aProcessor,
                             nsIAtom* aContainerVariable,
                             Test aContainer,
                             Test aEmpty);

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations,
                                          bool* aCantHandleYet) const;

    virtual bool
    CanPropagate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const;

    virtual void
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget) const;


    class Element : public MemoryElement {
    protected:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) CPP_THROW_NEW { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        Element(nsIRDFResource* aContainer,
                Test aContainerTest,
                Test aEmptyTest)
            : mContainer(aContainer),
              mContainerTest(aContainerTest),
              mEmptyTest(aEmptyTest) {
            MOZ_COUNT_CTOR(nsRDFConInstanceTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFConInstanceTestNode::Element); }

        static Element*
        Create(nsIRDFResource* aContainer,
               Test aContainerTest, Test aEmptyTest) {
            void* place = MemoryElement::gPool.Alloc(sizeof(Element));
            return place ? ::new (place) Element(aContainer, aContainerTest, aEmptyTest) : nsnull; }

        void Destroy() {
            this->~Element();
            MemoryElement::gPool.Free(this, sizeof(Element));
        }

        virtual const char* Type() const {
            return "nsRDFConInstanceTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return mozilla::HashGeneric(mContainerTest, mEmptyTest, mContainer.get());
        }

        virtual bool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = static_cast<const Element&>(aElement);
                return mContainer == element.mContainer
                    && mContainerTest == element.mContainerTest
                    && mEmptyTest == element.mEmptyTest;
            }
            return false; }

    protected:
        nsCOMPtr<nsIRDFResource> mContainer;
        Test mContainerTest;
        Test mEmptyTest;
    };

protected:
    nsXULTemplateQueryProcessorRDF* mProcessor;
    nsCOMPtr<nsIAtom> mContainerVariable;
    Test mContainer;
    Test mEmpty;
};

#endif // nsRDFConInstanceTestNode_h__

