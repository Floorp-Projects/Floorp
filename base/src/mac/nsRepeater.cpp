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

list<Repeater*> Repeater::sRepeaters;
list<Repeater*> Repeater::sIdlers;

Repeater::Repeater()
{
  mRepeating = false;
}

Repeater::~Repeater()
{
}

// repeater methods

void Repeater::StartRepeating()
{
  if (!mRepeating) 
  {
    sRepeaters.push_back(this);
    mRepeating = true;
  }
}

void Repeater::StopRepeating()
{
  if (mRepeating)
  {
    sRepeaters.remove(this);
    mRepeating = false;
  }
}

void Repeater::DoRepeaters(const EventRecord &aMacEvent)
{
  list<Repeater*>::iterator iter;
  for (iter = sRepeaters.begin(); iter != sRepeaters.end(); ++iter)
  {
    (*iter)->RepeatAction(aMacEvent);
  }
}

// idler methods

void Repeater::StartIdling()
{
  if (!mIdling) 
  {
    sIdlers.push_back(this);
    mIdling = true;
  }
}

void Repeater::StopIdling()
{
  if (mIdling)
  {
    sIdlers.remove(this);
    mIdling = false;
  }
}

void Repeater::DoIdlers(const EventRecord &aMacEvent)
{
  list<Repeater*>::iterator iter;
  for (iter = sIdlers.begin(); iter != sIdlers.end(); ++iter)
  {
    (*iter)->RepeatAction(aMacEvent);
  }
}



    
