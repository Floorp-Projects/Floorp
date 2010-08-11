/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2010 Mozilla Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation
 *
 * Contributor(s):
 *	Bas Schouten <bschouten@mozilla.com>
 */
#ifndef CAIRO_WIN32_REFPTR_H
#define CAIRO_WIN32_REFPTR_H

template<class T>
class RefPtr
{
public:
    RefPtr() : mPtr(NULL)
    { }

    RefPtr(T *aPtr) : mPtr(aPtr)
    {
	if (mPtr) {
	    mPtr->AddRef();
	}
    }

    RefPtr(const RefPtr<T> &aRefPtr)
    {
	mPtr = aRefPtr.mPtr;
	if (mPtr) {
	    mPtr->AddRef();
	}
    }

    template <class newType>
    explicit RefPtr(const RefPtr<newType> &aRefPtr)
    {
	mPtr = aRefPtr.get();
	if (mPtr) {
	    mPtr->AddRef();
	}
    }

    ~RefPtr()
    {
	if (mPtr) {
	    mPtr->Release();
	}
    }

    RefPtr<T> &operator =(const RefPtr<T> aPtr)
    {
	assignPtr(aPtr.mPtr);
	return *this;
    }
    
    RefPtr<T> &operator =(T* aPtr)
    {
	assignPtr(aPtr);
	return *this;
    }

    /** 
     * WARNING for ease of use, passing a reference will release/clear out ptr!
     * We null out the ptr before returning its address so people passing byref
     * as input will most likely get functions returning errors rather than accessing
     * freed memory. Further more accessing it after this point if it hasn't
     * been set will produce a null pointer dereference.
     */
    T** operator&()
    {
	if (mPtr) {
	    mPtr->Release();
	    mPtr = NULL;
	}
	return &mPtr;
    }

    T* operator->()
    {
	return mPtr;
    }

    T* operator->() const
    {
	return mPtr;
    }

    operator bool()
    {
	return (mPtr ? true : false);
    }

    operator T*()
    {
	return mPtr;
    }

    template <class newType>
    operator RefPtr<newType>()
    {
	RefPtr<newType> newPtr;
	newPtr = mPtr;
	return newPtr;
    }

    T* get() const
    {
	return mPtr;
    }

    T* forget()
    {
	T* ptr = mPtr;
	mPtr = NULL;
	return ptr;
    }

private:
    void assignPtr(T* aPtr)
    {
	T *oldPtr = mPtr;
	mPtr = aPtr;
	if (mPtr) {
	    mPtr->AddRef();
	}
	if (oldPtr) {
	    oldPtr->Release();
	}
    }

    T *mPtr;
};

#endif
