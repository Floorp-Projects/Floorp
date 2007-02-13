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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#include "MMgc.h"

using namespace std;

#ifdef OVERRIDE_GLOBAL_NEW

#ifdef _DEBUG
int cBlocksAllocated = 0;  // Count of blocks allocated.
#endif // _DEBUG

#ifndef __GNUC__

// User-defined operator new.
void *operator new(size_t size)
{
	#ifdef _DEBUG
		cBlocksAllocated++;
	#endif // _DEBUG

	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}

void *operator new[](size_t size)
{
	#ifdef _DEBUG
		cBlocksAllocated++;
	#endif

	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}

// User-defined operator delete.
#ifdef _MAC
// CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
// (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
void operator delete( void *p) throw()
#else
void operator delete( void *p)
#endif
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
	#ifdef _DEBUG
		cBlocksAllocated--;
	#endif // _DEBUG
}

#ifdef _MAC
// CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
// (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
void operator delete[]( void *p) throw()
#else
void operator delete[]( void *p )
#endif
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
	#ifdef _DEBUG
		cBlocksAllocated--;
	#endif // _DEBUG
}

#endif // __GNUC__
#endif // OVERRIDE_GLOBAL_NEW
