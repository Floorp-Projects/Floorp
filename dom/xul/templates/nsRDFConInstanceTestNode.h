/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFConInstanceTestNode_h__
#define nsRDFConInstanceTestNode_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
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
                                          bool* aCantHandleYet) const MOZ_OVERRIDE;

    virtual bool
    CanPropagate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const MOZ_OVERRIDE;

    virtual void
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget) const MOZ_OVERRIDE;


    class Element : public MemoryElement {
    public:
        Element(nsIRDFResource* aContainer,
                Test aContainerTest,
                Test aEmptyTest)
            : mContainer(aContainer),
              mContainerTest(aContainerTest),
              mEmptyTest(aEmptyTest) {
            MOZ_COUNT_CTOR(nsRDFConInstanceTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFConInstanceTestNode::Element); }

        virtual const char* Type() const MOZ_OVERRIDE {
            return "nsRDFConInstanceTestNode::Element"; }

        virtual PLHashNumber Hash() const MOZ_OVERRIDE {
            return mozilla::HashGeneric(mContainerTest, mEmptyTest, mContainer.get());
        }

        virtual bool Equals(const MemoryElement& aElement) const MOZ_OVERRIDE {
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

