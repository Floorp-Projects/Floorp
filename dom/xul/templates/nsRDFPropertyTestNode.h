/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFPropertyTestNode_h__
#define nsRDFPropertyTestNode_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsRDFTestNode.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsXULTemplateQueryProcessorRDF.h"

class nsRDFPropertyTestNode : public nsRDFTestNode
{
public:
    /**
     * Both source and target unbound (?source ^property ?target)
     */
    nsRDFPropertyTestNode(TestNode* aParent,
                          nsXULTemplateQueryProcessorRDF* aProcessor,
                          nsIAtom* aSourceVariable,
                          nsIRDFResource* aProperty,
                          nsIAtom* aTargetVariable);

    /**
     * Source bound, target unbound (source ^property ?target)
     */
    nsRDFPropertyTestNode(TestNode* aParent,
                          nsXULTemplateQueryProcessorRDF* aProcessor,
                          nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIAtom* aTargetVariable);

    /**
     * Source unbound, target bound (?source ^property target)
     */
    nsRDFPropertyTestNode(TestNode* aParent,
                          nsXULTemplateQueryProcessorRDF* aProcessor,
                          nsIAtom* aSourceVariable,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

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
        Element(nsIRDFResource* aSource,
                nsIRDFResource* aProperty,
                nsIRDFNode* aTarget)
            : mSource(aSource),
              mProperty(aProperty),
              mTarget(aTarget) {
            MOZ_COUNT_CTOR(nsRDFPropertyTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFPropertyTestNode::Element); }

        virtual const char* Type() const override {
            return "nsRDFPropertyTestNode::Element"; }

        virtual PLHashNumber Hash() const override {
            return mozilla::HashGeneric(mSource.get(), mProperty.get(), mTarget.get());
        }

        virtual bool Equals(const MemoryElement& aElement) const override {
            if (aElement.Type() == Type()) {
                const Element& element = static_cast<const Element&>(aElement);
                return mSource == element.mSource
                    && mProperty == element.mProperty
                    && mTarget == element.mTarget;
            }
            return false; }

    protected:
        nsCOMPtr<nsIRDFResource> mSource;
        nsCOMPtr<nsIRDFResource> mProperty;
        nsCOMPtr<nsIRDFNode> mTarget;
    };

protected:
    nsXULTemplateQueryProcessorRDF* mProcessor;
    nsCOMPtr<nsIAtom>        mSourceVariable;
    nsCOMPtr<nsIRDFResource> mSource;
    nsCOMPtr<nsIRDFResource> mProperty;
    nsCOMPtr<nsIAtom>        mTargetVariable;
    nsCOMPtr<nsIRDFNode>     mTarget;
};

#endif // nsRDFPropertyTestNode_h__
