/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txStack_h___
#define txStack_h___

#include "nsTArray.h"

class txStack : private nsTArray<void*>
{
public:
    /**
     * Returns the specified object from the top of this stack,
     * without removing it from the stack.
     *
     * @return a pointer to the object that is the top of this stack.
     */
    inline void* peek()
    {
        NS_ASSERTION(!isEmpty(), "peeking at empty stack");
        return !isEmpty() ? ElementAt(Length() - 1) : nullptr;
    }

    /**
     * Adds the specified object to the top of this stack.
     *
     * @param obj a pointer to the object that is to be added to the
     * top of this stack.
     */
    inline nsresult push(void* aObject)
    {
        return AppendElement(aObject) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    /**
     * Removes and returns the specified object from the top of this
     * stack.
     *
     * @return a pointer to the object that was the top of this stack.
     */
    inline void* pop()
    {
        void* object = nullptr;
        NS_ASSERTION(!isEmpty(), "popping from empty stack");
        if (!isEmpty())
        {
            const uint32_t count = Length() - 1;
            object = ElementAt(count);
            RemoveElementAt(count);
        }
        return object;
    }

    /**
     * Returns true if there are no objects in the stack.
     *
     * @return true if there are no objects in the stack.
     */
    inline bool isEmpty()
    {
        return IsEmpty();
    }

    /**
     * Returns the number of elements in the Stack.
     *
     * @return the number of elements in the Stack.
     */
    inline int32_t size()
    {
        return Length();
    }

private:
    friend class txStackIterator;
};

class txStackIterator
{
public:
    /**
     * Creates an iterator for the given stack.
     *
     * @param aStack the stack to create an iterator for.
     */
    inline
    explicit txStackIterator(txStack* aStack) : mStack(aStack),
                                                mPosition(0)
    {
    }

    /**
     * Returns true if there is more objects on the stack.
     *
     * @return .
     */
    inline bool hasNext()
    {
        return (mPosition < mStack->Length());
    }

    /**
     * Returns the next object pointer from the stack.
     *
     * @return .
     */
    inline void* next()
    {
        if (mPosition == mStack->Length()) {
            return nullptr;
        }
        return mStack->ElementAt(mPosition++);
    }

private:
    txStack* mStack;
    uint32_t mPosition;
};

#endif /* txStack_h___ */
