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
class cached
		/*
			...
		*/
	{
		public:

			cached()
					: _cached_value_is_valid(false)
				{
					// nothing else to do
				}

			explicit
			cached( const T& initial_value_to_cache )
					: _cached_value					(initial_value_to_cache),
						_cached_value_is_valid(true)
				{
					// nothing else to do
				}



			operator const T&() const
					// ...(not explicit) so you can treat a cached<T> as a T in _most_ cases.
				{
					return _cached_value;
				}

			cached<T>&
			operator=( const T& value )
					// ...assigning a value into a cache means 'cache this value'
				{
					set_cached_value(value);
					return *this;
				}

			cached<T>&
			operator=( const cached<T>& other )
					// ...otherwise assignment might make a valid copy of an invalid cache.
				{
					if ( (_cached_value_is_valid = other._cached_value_is_valid) == true )
						_cached_value = other._cached_value;
					return *this;
				}				


			bool
			is_valid() const
				{
					return _cached_value_is_valid;
				}

			void
			invalidate()
				{
					_cached_value_is_valid = false;
				}

		protected:

			void
			set_cached_value( const T& value )
				{
					_cached_value						= value;
					_cached_value_is_valid	= true;
				}

		private:
			T			_cached_value;
			bool	_cached_value_is_valid;
	};
