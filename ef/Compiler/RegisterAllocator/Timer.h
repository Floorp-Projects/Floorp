/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _TIMER_H_
#define _TIMER_H_

#include "Fundamentals.h"
#include "HashTable.h"
#include "prtime.h"

//
// Naming convention:
//  As the class Timer contains only static methods, the timer's name should start with the 
//  module name. Otherwise starting 2 timers with the same name will assert.
//

#ifndef NO_TIMER

struct TimerEntry
{
	PRTime *startTime;					// Current time when we start the timer.
	PRTime accumulator;					// Time spent in this timer.
	bool running;						// True if the timer is running.
	bool done;							// True if the timer was running and was stopped. 
};

class Timer
{
private:

	// Return the named timer.
	static TimerEntry& getTimerEntry(const char* name);
	// Return a reference to a new Timer.
	static PRTime& getNewTimer();

public:

	// Start the timer.
	static void start(const char* name);
	// Stop the timer.
	static void stop(const char* name);
	// Freeze all the running timers.
	static void freezeTimers();
	// Unfreeze all the running timers.
	static void unfreezeTimers();
	// Print the timer.
	static void print(FILE* f, const char *name);
};

inline void startTimer(const char* name) 		{Timer::start(name);}
inline void stopTimer(const char* name) 		{Timer::stop(name); Timer::print(stdout, name);}
#define START_TIMER_SAFE						Timer::freezeTimers();
#define END_TIMER_SAFE							Timer::unfreezeTimers();
#define TIMER_SAFE(x)							START_TIMER_SAFE x; END_TIMER_SAFE

#else /* NO_TIMER */

inline void startTimer(const char* /*name*/) 	{} 
inline void stopTimer(const char* /*name*/) 	{} 
#define START_TIMER_SAFE
#define END_TIMER_SAFE
#define TIMER_SAFE(x)							x;

#endif /* NO_TIMER */
#endif /* _TIMER_H_ */
