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

#ifndef nsRepeater_h___
#define nsRepeater_h___

class EventRecord;

class Repeater {
  public:
  
    Repeater();
    virtual ~Repeater();
    
    virtual	void RepeatAction(const EventRecord &aMacEvent) = 0;
    
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