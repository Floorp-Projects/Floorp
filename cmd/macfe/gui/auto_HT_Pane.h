/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// auto_HT_Pane.h

#pragma once

#include <memory>			// for the definition of |auto_ptr| before specializing it
#include "htrdf.h"		// for the definition of |HT_Pane| and the declaration of |HT_DeletePane|

	/*
		Please note the usual admonition: An |auto_ptr| is a the owner of a given resource.
		Certain syntactic liberties have been taken so that functions that return |auto_ptr|s
		can yield ownership in expressions like this where a |const| temporary might otherwise
		interfere
		
			auto_ptr<X> create_X();
		  auto_ptr<X> x;
		  // ...
		  x = create_X();

		The machinery that allows that assignment to succeed will make other, less desirable
		assigments succeed as well.  To avoid problems, remember this rule of thumb

      "If you don't intend to give away ownership, don't show them your |auto_ptr|."

		Give them a reference (e.g., |*x|), give them a pointer (e.g., |x.get()|), but don't
		even think of giving them an |auto_ptr| or a reference to it, EVEN A CONST REFERENCE,
		because you may end up yielding ownership.

		Additionally, when you write the equivalent of |create_X|, above, try to write to
		facilitate the return-value optimization, e.g.,

			auto_ptr<X>
			create_X()
				{
					return auto_ptr<X>(new X)
				}
	*/

class auto_ptr<_HT_PaneStruct>
		/*
			...specializes |auto_ptr| template to deal with |HT_Pane| which is
			really a |_HT_PaneStruct*|.  This class should be transparent to anyone
			who already knows how |auto_ptr| works, and already thinks of |HT_Pane|
			as a pointer.

			This class implements the Nov '97 version of the C++ standard, but
			will require some small modifications when member templates are fully supported.
		*/
	{
		public:

			struct auto_ptr_ref
				{
					auto_ptr& p_;

					auto_ptr_ref( const auto_ptr& a )
							: p_( const_cast<auto_ptr&>(a) )
						{
							// nothing else to do
						}
				};

		public:
			typedef _HT_PaneStruct element_type;

			explicit
			auto_ptr( HT_Pane P = 0 )
					: _pane(P)
				{
					// nothing else to do
				}

			auto_ptr( auto_ptr& a )
					: _pane(a.release())
				{
					// nothing else to do
				}

			~auto_ptr()
		 		{
		 			if ( _pane )
		 				HT_DeletePane(_pane);
		 		}

			auto_ptr&
			operator=( auto_ptr& a )
				{
					reset(a.release());
					return *this;
				}

			_HT_PaneStruct&
			operator*() const
				{
					return *get();
				}

			HT_Pane
			operator->() const
				{
					return get();
				}

			HT_Pane
			get() const
				{
					return _pane;
				}

			HT_Pane
			release()
				{
					HT_Pane result = get();
					_pane = 0;
					return result;
				}

			void
			reset( HT_Pane P = 0 )
				{
					if ( get() != P )
						{
							if ( _pane )
								HT_DeletePane(_pane);
							_pane = P;
						}
				}

			auto_ptr( auto_ptr_ref r )
					: _pane(r.p_.release())
				{
					// nothing else to do
				}

				// not in the C++ standard (yet?), but needed for the same reason as the method above
			auto_ptr&
			operator=( auto_ptr_ref r )
				{
					reset(r.p_.release());
					return *this;
				}

			operator auto_ptr_ref()
				{
					return *this;
				}

		private:
			HT_Pane	_pane;
	};


