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

#ifndef nsRDFPropertyTestNode_h__
#define nsRDFPropertyTestNode_h__

#include "nsFixedSizeAllocator.h"
#include "nsRDFTestNode.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
class nsConflictSet;

class nsRDFPropertyTestNode : public nsRDFTestNode
{
public:
    /**
     * Both source and target unbound (?source ^property ?target)
     */
    nsRDFPropertyTestNode(InnerNode* aParent,
                          nsConflictSet& aConflictSet,
                          nsIRDFDataSource* aDataSource,
                          PRInt32 aSourceVariable,
                          nsIRDFResource* aProperty,
                          PRInt32 aTargetVariable);

    /**
     * Source bound, target unbound (source ^property ?target)
     */
    nsRDFPropertyTestNode(InnerNode* aParent,
                          nsConflictSet& aConflictSet,
                          nsIRDFDataSource* aDataSource,
                          nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          PRInt32 aTargetVariable);

    /**
     * Source unbound, target bound (?source ^property target)
     */
    nsRDFPropertyTestNode(InnerNode* aParent,
                          nsConflictSet& aConflictSet,
                          nsIRDFDataSource* aDataSource,
                          PRInt32 aSourceVariable,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

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
        Element(nsIRDFResource* aSource,
                nsIRDFResource* aProperty,
                nsIRDFNode* aTarget)
            : mSource(aSource),
              mProperty(aProperty),
              mTarget(aTarget) {
            MOZ_COUNT_CTOR(nsRDFPropertyTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsRDFPropertyTestNode::Element); }

        static Element*
        Create(nsFixedSizeAllocator& aPool,
               nsIRDFResource* aSource,
               nsIRDFResource* aProperty,
               nsIRDFNode* aTarget) {
            void* place = aPool.Alloc(sizeof(Element));
            return place ? ::new (place) Element(aSource, aProperty, aTarget) : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, Element* aElement) {
            aElement->~Element();
            aPool.Free(aElement, sizeof(*aElement)); }

        virtual const char* Type() const {
            return "nsRDFPropertyTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(mSource.get()) ^
                (PLHashNumber(mProperty.get()) >> 4) ^
                (PLHashNumber(mTarget.get()) >> 12); }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mSource == element.mSource
                    && mProperty == element.mProperty
                    && mTarget == element.mTarget;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone(void* aPool) const {
            return Create(*NS_STATIC_CAST(nsFixedSizeAllocator*, aPool),
                          mSource, mProperty, mTarget); }

    protected:
        nsCOMPtr<nsIRDFResource> mSource;
        nsCOMPtr<nsIRDFResource> mProperty;
        nsCOMPtr<nsIRDFNode> mTarget;
    };

protected:
    nsConflictSet&             mConflictSet;
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    PRInt32                  mSourceVariable;
    nsCOMPtr<nsIRDFResource> mSource;
    nsCOMPtr<nsIRDFResource> mProperty;
    PRInt32                  mTargetVariable;
    nsCOMPtr<nsIRDFNode>     mTarget;
};

#endif // nsRDFPropertyTestNode_h__
