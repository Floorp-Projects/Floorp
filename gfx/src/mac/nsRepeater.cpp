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
 

/* to build this, build mozilla/netwerk/util/netwerkUtil.mcp
 * I hope I'm the last person to waste 30 minutes looking for the project that builds this file */
#pragma export on

#include "nsRepeater.h"

Repeater* Repeater::sRepeaters = NULL;
Repeater* Repeater::sIdlers = NULL;

Repeater::Repeater()
: mRepeating(false)
, mIdling(false)
, mPrevRptr(NULL)
, mNextRptr(NULL)
, mPrevIdlr(NULL)
, mNextIdlr(NULL)
{
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
  mPrevRptr = NULL;
  mNextRptr = NULL;
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
  mPrevIdlr = NULL;
  mNextIdlr = NULL;
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
    theIdler->IdleAction(aMacEvent);
    theIdler = nextIdler;
  }
}


#pragma export off
