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

void Repeater::AddToRepeatList()
{
  if (sRepeaters)
  {
    sRepeaters->mPrevRptr = this;
    mNextRptr = sRepeaters;
  }
  sRepeaters = this;
}
void Repeater::RemoveFromRepeatList()
{
  if (sRepeaters == this) sRepeaters = mNextRptr;
  if (mPrevRptr) mPrevRptr->mNextRptr = mNextRptr;
  if (mNextRptr) mNextRptr->mPrevRptr = mPrevRptr;
  mPrevRptr = 0;
  mNextRptr = 0;
}
void Repeater::AddToIdleList()
{
  if (sRepeaters)
  {
    sRepeaters->mPrevIdlr = this;
    mNextIdlr = sRepeaters;
  }
  sRepeaters = this;
}
void Repeater::RemoveFromIdleList()
{
  if (sRepeaters == this) sRepeaters = mNextIdlr;
  if (mPrevIdlr) mPrevIdlr->mNextIdlr = mNextIdlr;
  if (mNextIdlr) mNextIdlr->mPrevIdlr = mPrevIdlr;
  mPrevIdlr = 0;
  mNextIdlr = 0;
}

// repeater methods

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
    theRepeater->RepeatAction(aMacEvent);
    theRepeater = theRepeater->mNextRptr;
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
    theIdler->RepeatAction(aMacEvent);
    theIdler = theIdler->mNextIdlr;
  }
}



    
