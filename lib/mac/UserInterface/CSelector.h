/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#pragma once

#include "opaque_ptr.h"
#include "own_ptr.h"
#include "cached.h"
#include <list.h>
#include <algo.h>

/*
	Note:
		some warnings are generated because a list of items that have |own_ptr| members doesn't like
		the template constructor that builds from a const reference to another such list.  I'll try to
		do the right thing soon...  one can't construct an |own_ptr| from a |const own_ptr&| since the
		source object must yield ownership, this changing state.
*/

#pragma mark -- class CSelector --


/*
	CSelector
	
	A pane displaying a list of `selectors' (icons w/labels).

	With this API, at most one selector can be `active', i.e., the currently selected one.  If we decide to implement
	multiple selection, then the right thing to do might be: rethink `active' or `selected' as `should be shown', and
	put a |bool| to that effect in the |Selector| struct.

	Whenever the active selector changes, we |BroadcastMessage(msg_ActiveSelectorChanged, this)|.
	Clients then use something like |static_cast<const CNavCenterSelectorPane*>(ioParam)->active_selector()|
	to get the new active selector.

	To reduce the volume of code and APIs, clients can ask for the underlying list of selectors displayed by the pane,
	and make changes directly to that list.  Right now they get an actual reference to the list.  I want to change that
	to some kind of `ticket' that acts like a reference to the list, but automatically notifies the pane that the
	list has changed when the ticket is destucted.  In the mean time, after a client has modified the list, but before
	they have called |notice_selectors_changed()|, the pane will not acurately reflect the contents of the list, and
	the results of |active_selector(void)| are not necessarily valid.  See below.
	
	This class knows nothing about PowerPlant or the macFE. It could be (and maybe should be)
	XP code if we so desired.
*/

template<class T>	
class CSelector 
{
public:

		// bitfield constants for the image mode
	enum {
		kSelected = 1, kIcon = 2, kText = 4
	};

	typedef list<T>							SelectorList;
	typedef SelectorList::iterator			iterator;
	typedef SelectorList::const_iterator	const_iterator;


	class equals_selector
			// ...a predicate for finding a particular selector, given an |opaque_ptr<Selector>|.
		{
			public:
				explicit
				equals_selector( opaque_ptr<T> ptr_to_desired_selector )
						: _ptr_to_desired_selector(ptr_to_desired_selector)
					{
						// nothing else to do
					}

				bool
				operator()( const T& selector )
					{
						return opaque_ptr<T>(&selector) == _ptr_to_desired_selector;
					}

			private:
				opaque_ptr<T> _ptr_to_desired_selector;
		};

		
		// constructor and destructor
	CSelector ( unsigned long inImageMode, size_t inCellHeight )
					: _image_mode(inImageMode), _cell_height(inCellHeight) { };
	virtual ~CSelector ( ) { };
	
	
	const SelectorList&
	selectors() const
			// Return the list of |Selector|s for clients to _examine_.
		{
			return _selector_list;
		}

	SelectorList&
	selectors()
			// Return the list of |Selector|s for clients to _modify_.  Clients must call |notice_selectors_changed()| if they do.
		{
			return _selector_list;
		}

	virtual void notice_selectors_changed();
			// Clients call after modifying the list of |Selector|s.  Among other things, the page will |Refresh()|.

		/*
			Note: |notice_selectors_changed()| will notice if the active selector has been removed from the list
				and will broadcast that the selector has changed, i.e., you are saying `currently, no selector is active'.
			  If you were about to activate a new selector, then you should call |active_selector(iter)| _before_
			  calling |notice_selectors_changed()| to minimize the number of broadcasts.
		*/

	iterator find_active_selector();
			// Return (an |iterator| pointing to) the active selector, or |selectors().end()| if no selector is active.

	const_iterator find_active_selector() const
			// Return (a |const_iterator| pointing to) the active selector, or |selectors().end()| if no selector is active.
		{
			return const_cast<CSelector*>(this)->find_active_selector();
		}

	iterator active_selector()
			/*
				Return (an |iterator| pointing to) the active selector, or |selectors().end()| if no selector is active.
				The result may dangle if |notice_selectors_changed()| is pending (i.e., hasn't yet been called, though it needs to).
			*/
		{
			if ( !_cached_active_selector.is_valid() )
				_cached_active_selector = find_active_selector();
			return _cached_active_selector;
		}

	const_iterator active_selector() const
			/*
				Return (a |const_iterator| pointing to) the active selector, or |selectors().end()| if no selector is active.
			  The result may dangle if |notice_selectors_changed()| is pending (i.e., hasn't yet been called, though it needs to).
			*/
		{
			return const_cast<CSelector*>(this)->active_selector();
		}

		/*
			The difference between |find_active_selector()| and |active_selector()| is that the latter may be faster,
			but could return a dangling iterator if a client has erased the currently active selector from the
			selector list (possibly as part of a larger change) and not yet called |notice_selectors_changed()|.
			There are no guarantees for |active_selector()| while |notice_selectors_changed()| is pending (i.e., hasn't
			yet been called, though a client has modified the list).

			Both return an iterator equal to |selectors().end()| if there is no currently active selector (not guaranteed
			in the case where |active_selector()| returns a dangling iterator).

			The mnemonic for the difference is that |find_active_selector| actually has to go out and find it, while
			|active_selector| can simply return a value.
		*/

	void active_selector( const_iterator iter );
			// Make |*iter| the active |Selector|, and if that's a change then broadcast |msg_ActiveSelectorChanged| and |Refresh()| the pane.

	virtual const_iterator find_selector_for_point ( const Point & inMouseLoc ) const ;
	virtual size_t find_index_for_point ( const Point & inMouseLoc ) const ;

protected:

	virtual void notice_active_selector_changed ( const_iterator inOldSelector );
	
		// override to actually make use of a broadcaster implementation
	virtual void broadcast_active_selector_changed ( ) = 0;

	SelectorList					_selector_list;
	opaque_ptr<T>					_active_selector;
	cached<iterator>				_cached_active_selector;

	unsigned long					_image_mode;
	size_t							_cell_height;

}; // class CSelector


#pragma mark -- IMPLEMENTATION --


template <class T>
void
CSelector<T>::notice_selectors_changed()
{
		// Check that the active selector wasn't removed from this list...
	const_iterator new_active_selector = find_active_selector();
	if ( new_active_selector != _active_selector )
		active_selector(new_active_selector);	// ...it was!  We'd better tell everybody, there is no active selection.
}

template <class T>
CSelector<T>::iterator
CSelector<T>::find_active_selector()
{
	return find_if(selectors().begin(), selectors().end(), equals_selector(_active_selector));
}


template <class T>
void
CSelector<T>::active_selector( const_iterator iter )
{
	const_iterator old_selector = active_selector();		// save old one
	_active_selector = opaque_ptr<T>(&(*iter));
	notice_active_selector_changed(old_selector);
}


template <class T>
void
CSelector<T>::notice_active_selector_changed ( const_iterator /*inNewSelector*/ )
{		
	_cached_active_selector.invalidate();
	broadcast_active_selector_changed ( );		
}


template <class T>
size_t
CSelector<T> :: find_index_for_point ( const Point & inMouseLoc ) const
{
	return inMouseLoc.v / _cell_height;
}


//
// find_selector_for_point
//
// Given a point in image coordinates, return which selector the mouse
// is over.
//
template <class T>
CSelector<T>::const_iterator
CSelector<T> :: find_selector_for_point ( const Point & inMouseLoc ) const
{
	size_t cell_index = find_index_for_point ( inMouseLoc );
	if ( cell_index > selectors().size() )
		return selectors().end();
		
	const_iterator iter = selectors().begin();
	advance ( iter, cell_index );
			
	return iter;

} // find_selector_for_point
