/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#pragma export on
#include "nsRepeater.h"

Repeater* Repeater::sRepeaters = 0;
Repeater* Repeater::sIdlers = 0;

Repeater::Repeater()
{
  mRepeating = false;
  mIdling = false;
  mPrevRptr = 0;
  mNextRptr = 0;
  mPrevIdlr = 0;
  mNextIdlr = 0;
  
}

Repeater::~Repeater()
{
  if (mRepeating) RemoveFromRepeatList();
  if (mIdling) RemoveFromIdleList();
  
}

// protected helper functs

//----------------------------------------------------------------------------
void Repeater::AddToRepeatList()
{
  if (sRepeaters)
  {
    sRepeaters->mPrevRptr = this;
    mNextRptr = sRepeaters;
  }
  sRepeaters = this;
}

//----------------------------------------------------------------------------
void Repeater::RemoveFromRepeatList()
{
  if (sRepeaters == this) sRepeaters = mNextRptr;
  if (mPrevRptr) mPrevRptr->mNextRptr = mNextRptr;
  if (mNextRptr) mNextRptr->mPrevRptr = mPrevRptr;
  mPrevRptr = 0;
  mNextRptr = 0;
}

//----------------------------------------------------------------------------
void Repeater::AddToIdleList()
{
  if (sIdlers)
  {
    sIdlers->mPrevIdlr = this;
    mNextIdlr = sIdlers;
  }
  sIdlers = this;
}

//----------------------------------------------------------------------------
void Repeater::RemoveFromIdleList()
{    
  if (sIdlers == this) sIdlers = mNextIdlr;
  if (mPrevIdlr) mPrevIdlr->mNextIdlr = mNextIdlr;
  if (mNextIdlr) mNextIdlr->mPrevIdlr = mPrevIdlr;
  mPrevIdlr = 0;
  mNextIdlr = 0;
}

// repeater methods
//----------------------------------------------------------------------------

void Repeater::StartRepeating()
{
  if (!mRepeating) 
  {
    AddToRepeatList();
    mRepeating = true;
  }
}

void Repeater::StopRepeating()
{
  if (mRepeating)
  {
    RemoveFromRepeatList();
    mRepeating = false;
  }
}

void Repeater::DoRepeaters(const EventRecord &aMacEvent)
{
  Repeater* theRepeater = sRepeaters;
  while (theRepeater)
  {
    Repeater* nextRepeater = theRepeater->mNextRptr;
    theRepeater->RepeatAction(aMacEvent);
    theRepeater = nextRepeater;
  }
}

// idler methods

void Repeater::StartIdling()
{
  if (!mIdling) 
  {
    AddToIdleList();
    mIdling = true;
  }
}

void Repeater::StopIdling()
{
  if (mIdling)
  {
    RemoveFromIdleList();
    mIdling = false;
  }
}

void Repeater::DoIdlers(const EventRecord &aMacEvent)
{
  Repeater* theIdler = sIdlers;
  while (theIdler)
  {
    Repeater* nextIdler = theIdler->mNextIdlr;
    theIdler->RepeatAction(aMacEvent);
    theIdler = nextIdler;
  }
}


#pragma export off


    
