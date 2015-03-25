/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFConMemberTestNode_h__
#define nsRDFConMemberTestNode_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsRDFTestNode.h"
#include "nsIRDFDataSource.h"
#include "nsXULTemplateQueryProcessorRDF.h"

/**
 * Rule network node that test if a resource is a member of an RDF
 * container, or is ``contained'' by another resource that refers to
 * it using a ``containment'' attribute.
 */
class nsRDFConMemberTestNode : public nsRDFTestNode
{
public:
    nsRDFConMemberTestNode(TestNode* aParent,
                           nsXULTemplateQueryProcessorRDF* aProcessor,
                           nsIAtom* aContainerVariable,
                           nsIAtom* aMemberVariable);

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations,
                                          bool* aCantHandleYet) const override;

    virtual bool
    CanPropagate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const override;

    virtual void
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget) const override;

    class Element : public MemoryElement {
    public:
        Element(nsIRDFResource* aContainer,
                nsIRDFNode* aMember)
            : mContainer(aContainer),
              mMember(aMember) {
            MOZ_COUNT_CTOR(nsRDFConMemberTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFConMemberTestNode::Element); }

        virtual const char* Type() const override {
            return "nsRDFConMemberTestNode::Element"; }

        virtual PLHashNumber Hash() const override {
            return PLHashNumber(NS_PTR_TO_INT32(mContainer.get())) ^
                (PLHashNumber(NS_PTR_TO_INT32(mMember.get())) >> 12); }

        virtual bool Equals(const MemoryElement& aElement) const override {
            if (aElement.Type() == Type()) {
                const Element& element = static_cast<const Element&>(aElement);
                return mContainer == element.mContainer && mMember == element.mMember;
            }
            return false; }

    protected:
        nsCOMPtr<nsIRDFResource> mContainer;
        nsCOMPtr<nsIRDFNode> mMember;
    };

protected:
    nsXULTemplateQueryProcessorRDF* mProcessor;
    nsCOMPtr<nsIAtom> mContainerVariable;
    nsCOMPtr<nsIAtom> mMemberVariable;
};

#endif // nsRDFConMemberTestNode_h__
