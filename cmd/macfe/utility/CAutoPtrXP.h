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

/*	CAutoPtrXP.h */

#ifndef CAutoPtrXP_H
#define CAutoPtrXP_H
#pragma once

#include "xp_mem.h"

template<class T>
class CAutoPtrXP
{
	public:
		CAutoPtrXP(T* p = 0) : p_(p) { }
		
		CAutoPtrXP(const CAutoPtrXP& r) : p_(r.release()) { }
		
		~CAutoPtrXP() {
			if (p_)
			{
				XP_FREE(p_);
			}
		}
		
		CAutoPtrXP& operator = (const CAutoPtrXP& r) {
			if ((void *)&r != (void *) this) {
				if (p_)
				{
					XP_FREE(p_);
				}
				p_ = r.release();
			}
			return *this;
		}

		T& operator*(void) const{ return *p_; }
		
		T* operator->(void) const { return p_; }
		
		T* get(void) const { return p_; }
		
		void reset(T* p = 0) {
			if (p_)
			{
				XP_FREE(p_);
			}
			p_ = p;
		}
		
		T* release() const { 
			T* old = p_;
			const_cast<CAutoPtrXP*>(this)->p_ = 0;
			return old; 
		}

	private:
		T*	p_;
};

#endif