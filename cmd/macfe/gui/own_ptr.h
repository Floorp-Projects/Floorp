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

#pragma once

template <class T>
class own_ptr
		/*
			...implements strict ownership model (as |auto_ptr| once did, before it was ruined).
		*/
	{
		public:

			explicit
			own_ptr( T* ptr = 0 )
					: _ptr(ptr)
				{
					// nothing else to do
				}

			own_ptr( own_ptr<T>& P )
					: _ptr( P.release() )
				{
					// nothing else to do
				}

			~own_ptr()
				{
					delete _ptr;
				}


			own_ptr<T>&
			operator=( T* P )
				{
					if ( _ptr != P )
						{
							delete _ptr;
							_ptr = P;
						}
					return *this;
				}

			own_ptr<T>&
			operator=( own_ptr<T>& P )
				{
					return operator=( P.release() );	// (also) handles the self-assignment case
				}

			T&
			operator*() const
				{
					// PRECONDITION( _ptr );

//#if !defined(__NO_bad_dereference__)
//					if ( !_ptr )
//						throw bad_dereference();
//#endif
					return *_ptr;
				}

			T*
			operator->() const
				{
					// PRECONDITION( _ptr );

//#if !defined(__NO_bad_dereference__)
//					if ( !_ptr )
//						throw bad_dereference();
//#endif
					return _ptr;
				}

			T*
			get() const
				{
					return _ptr;
				}

			T*
			release()
				{
					T* P = _ptr;
					_ptr = 0;
					return P;
				}

			bool
			is_void() const
				{
					return _ptr == 0;
				}



		private:
			T* _ptr;
	};
