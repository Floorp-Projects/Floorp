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

#ifndef nsRepeater_h___
#define nsRepeater_h___

#include "nscore.h"

class EventRecord;

class NS_NET Repeater
{
  public:
  
    Repeater();
    virtual ~Repeater();
    
    virtual	void RepeatAction(const EventRecord &aMacEvent) {}
    virtual	void IdleAction(const EventRecord &aMacEvent) {}
    
    void StartRepeating();
    void StopRepeating();
    void StartIdling();
    void StopIdling();
    
    static void DoRepeaters(const EventRecord &aMacEvent);
    static void DoIdlers(const EventRecord &aMacEvent);
    
  protected:
    
    void AddToRepeatList();
    void RemoveFromRepeatList();
    void AddToIdleList();
    void RemoveFromIdleList();
    
    static Repeater* sRepeaters;
    static Repeater* sIdlers;
    
    bool mRepeating;
    bool mIdling;
    Repeater* mPrevRptr;
    Repeater* mNextRptr;
    Repeater* mPrevIdlr;
    Repeater* mNextIdlr;
};

#endif
