/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        return !isEmpty() ? ElementAt(Length() - 1) : nsnull;
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
        void* object = nsnull;
        NS_ASSERTION(!isEmpty(), "popping from empty stack");
        if (!isEmpty())
        {
            const PRUint32 count = Length() - 1;
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
    inline PRInt32 size()
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
    txStackIterator(txStack* aStack) : mStack(aStack),
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
            return nsnull;
        }
        return mStack->ElementAt(mPosition++);
    }

private:
    txStack* mStack;
    PRUint32 mPosition;
};

#endif /* txStack_h___ */
