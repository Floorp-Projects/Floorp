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

#ifndef nsContentTestNode_h__
#define nsContentTestNode_h__

#include "nscore.h"
#include "nsRuleNetwork.h"
#include "nsFixedSizeAllocator.h"
#include "nsIAtom.h"

class nsIXULDocument;
class nsConflictSet;

class nsContentTestNode : public TestNode
{
public:
    nsContentTestNode(InnerNode* aParent,
                      nsConflictSet& aConflictSet,
                      nsIXULDocument* aDocument,
                      nsIContent* aRoot,
                      PRInt32 aContentVariable,
                      PRInt32 aIdVariable,
                      nsIAtom* aTag);

    virtual nsresult
    FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const;

    virtual nsresult
    GetAncestorVariables(VariableSet& aVariables) const;

    class Element : public MemoryElement {
    private:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        Element(nsIContent* aContent)
            : mContent(aContent) {
            MOZ_COUNT_CTOR(nsContentTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(nsContentTestNode::Element); }

        static Element*
        Create(nsFixedSizeAllocator& aPool, nsIContent* aContent) {
            void* place = aPool.Alloc(sizeof(Element));
            return place ? ::new (place) Element(aContent) : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, Element* aElement) {
            aElement->~Element();
            aPool.Free(aElement, sizeof(*aElement)); }

        virtual const char* Type() const {
            return "nsContentTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(NS_PTR_TO_INT32(mContent.get())) >> 2; }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mContent == element.mContent;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone(void* aPool) const {
            return Create(*NS_STATIC_CAST(nsFixedSizeAllocator*, aPool), mContent); }

    protected:
        nsCOMPtr<nsIContent> mContent;
    };

protected:
    nsConflictSet& mConflictSet;
    nsIXULDocument* mDocument; // [WEAK] because we know the document will outlive us
    nsCOMPtr<nsIContent> mRoot;
    PRInt32 mContentVariable;
    PRInt32 mIdVariable;
    nsCOMPtr<nsIAtom> mTag;
};

extern PRBool
IsElementContainedBy(nsIContent* aElement, nsIContent* aContainer);

#endif // nsContentTestNode_h__

