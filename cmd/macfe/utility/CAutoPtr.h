/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*	CAutoPtr.h	*/


#ifndef CAutoPtr_H
#define CAutoPtr_H
#pragma once

template<class T>
class CAutoPtr
{
	public:
		CAutoPtr(T* p = 0) : p_(p) { }
		
		CAutoPtr(const CAutoPtr& r) : p_(r.release()) { }
		
		~CAutoPtr() { delete p_; }
		
		CAutoPtr& operator = (const CAutoPtr& r) {
			if ((void *)&r != (void *) this) {
				delete p_;
				p_ = r.release();
			}
			return *this;
		}

		T& operator*(void) const{ return *p_; }
		
		T* operator->(void) const { return p_; }
		
		T* get(void) const { return p_; }
		
		void reset(T* p = 0) {
			delete p_;
			p_ = p;
		}
		
		T* release(void) const { 
			T* old = p_;
			const_cast<CAutoPtr*>(this)->p_ = 0;
			return old; 
		}

	private:
		T*	p_;
};

#endif