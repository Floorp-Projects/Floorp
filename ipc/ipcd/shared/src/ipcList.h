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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef ipcList_h__
#define ipcList_h__

#include "prtypes.h"

//-----------------------------------------------------------------------------
// simple list of singly-linked objects.  class T must have the following
// structure:
//
//   class T {
//   ...
//   public:
//     T *mNext;
//   };
//
// objects added to the list must be allocated with operator new.
//-----------------------------------------------------------------------------

template<class T>
class ipcList
{
public:
    ipcList()
        : mHead(NULL)
        , mTail(NULL)
        { }
   ~ipcList() { DeleteAll(); }

    //
    // prepends obj at the beginning of the list.
    //
    void Prepend(T *obj)
    {
        obj->mNext = mHead;
        mHead = obj;
        if (!mTail)
            mTail = mHead;
    }

    //
    // appends obj to the end of the list.
    //
    void Append(T *obj)
    {
        obj->mNext = NULL;
        if (mTail) {
            mTail->mNext = obj;
            mTail = obj;
        }
        else
            mTail = mHead = obj;
    }

    //
    // inserts b into the list after a.
    //
    void InsertAfter(T *a, T *b)
    {
        b->mNext = a->mNext;
        a->mNext = b;
        if (mTail == a)
            mTail = b;
    }

    // 
    // removes first element w/o deleting it
    //
    void RemoveFirst()
    {
        if (mHead)
            AdvanceHead();
    }

    //
    // removes element after the given element w/o deleting it
    //
    void RemoveAfter(T *obj)
    {
        T *rej = obj->mNext;
        if (rej) {
            obj->mNext = rej->mNext;
            if (rej == mTail)
                mTail = obj;
        }
    }

    //
    // deletes first element
    //
    void DeleteFirst()
    {
        T *first = mHead;
        if (first) {
            AdvanceHead();
            delete first;
        }
    }

    //
    // deletes element after the given element
    //
    void DeleteAfter(T *obj)
    {
        T *rej = obj->mNext;
        if (rej) {
            RemoveAfter(obj);
            delete rej;
        }
    }

    //
    // deletes all elements
    //
    void DeleteAll()
    {
        while (mHead)
            DeleteFirst();
    }

    const T *First() const { return mHead; }
    T       *First()       { return mHead; }
    const T *Last() const  { return mTail; }
    T       *Last()        { return mTail; }

    PRBool  IsEmpty() const { return mHead == NULL; }

protected:
    void AdvanceHead()
    {
        mHead = mHead->mNext;
        if (!mHead)
            mTail = NULL;
    }

    T *mHead;
    T *mTail;
};

#endif // !ipcList_h__
