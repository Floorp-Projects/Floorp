/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGraphicsStateBeOS.h"

//////////////////////////////////////////////////////////////////////////
nsGraphicsState::nsGraphicsState()
{
  mMatrix = nsnull;
  mClipRegion = nsnull;
  mColor = 0;
  mLineStyle = nsLineStyle_kSolid;
  mFontMetrics = nsnull;
}
//////////////////////////////////////////////////////////////////////////
nsGraphicsState::~nsGraphicsState()
{
  NS_IF_RELEASE(mFontMetrics);
}
//////////////////////////////////////////////////////////////////////////
nsGraphicsStatePool::nsGraphicsStatePool()
{
	mFreeList = nsnull;
}
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
//
// Public nsGraphicsStatePool
//
//////////////////////////////////////////////////////////////////////////
/* static */ nsGraphicsState *
nsGraphicsStatePool::GetNewGS()
{
  nsGraphicsStatePool * thePool = PrivateGetPool();

  return thePool->PrivateGetNewGS();
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
nsGraphicsStatePool::ReleaseGS(nsGraphicsState* aGS)
{
  nsGraphicsStatePool * thePool = PrivateGetPool();

  thePool->PrivateReleaseGS(aGS);
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
// Private nsGraphicsStatePool
//
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
nsGraphicsStatePool *
nsGraphicsStatePool::gsThePool = nsnull;

//////////////////////////////////////////////////////////////////////////
nsGraphicsStatePool *
nsGraphicsStatePool::PrivateGetPool()
{
  if (nsnull == gsThePool)
  {
    gsThePool = new nsGraphicsStatePool();
  }

  return gsThePool;
}

//////////////////////////////////////////////////////////////////////////

nsGraphicsStatePool::~nsGraphicsStatePool()
{
	nsGraphicsState* gs = mFreeList;
	while (gs != nsnull) {
		nsGraphicsState* next = gs->mNext;
		delete gs;
		gs = next;
	}
}
//////////////////////////////////////////////////////////////////////////
nsGraphicsState *
nsGraphicsStatePool::PrivateGetNewGS()
{
	nsGraphicsState* gs = mFreeList;
	if (gs != nsnull) {
		mFreeList = gs->mNext;
		return gs;
	}
	return new nsGraphicsState;
}
//////////////////////////////////////////////////////////////////////////
void
nsGraphicsStatePool::PrivateReleaseGS(nsGraphicsState* aGS)
{
  //	aGS->Clear();
	aGS->mNext = mFreeList;
	mFreeList = aGS;
}
//////////////////////////////////////////////////////////////////////////

