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

#include "Fundamentals.h"
#include "HashTable.h"
#include "Timer.h"
#include "Pool.h"

static Pool pool;											// Pool for the Timer class.
static HashTable<TimerEntry*> timerEntries(pool);			// Timers hashtable.

const nTimersInABlock = 128;								// Number of timers in a block.
static PRTime *timers = new(pool) PRTime[nTimersInABlock];	// A block of timers.
static Uint8 nextTimer = 0;									// nextAvailableTimer.

//
// Calibrate the call to PR_Now().
//
static PRTime calibrate()
{
	PRTime t = PR_Now();
	PRTime& a = *new(pool) PRTime();

	// Call 10 times the PR_Now() function.
	a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); 
	a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); a = PR_Now(); 
	t = (PR_Now() - t + 9) / 10;

	return t;
}

static PRTime adjust = calibrate();

//
// Return the named timer..
//
TimerEntry& Timer::getTimerEntry(const char* name)
{
	if (!timerEntries.exists(name)) {
		TimerEntry* newEntry = new(pool) TimerEntry();
		newEntry->accumulator = 0;
		newEntry->running = false;
		timerEntries.add(name, newEntry);
	}

	return *timerEntries[name];
}

//
// Return a reference to a new timer.
//
PRTime& Timer::getNewTimer()
{
	if (nextTimer >= nTimersInABlock) {
		timers = new(pool) PRTime[nTimersInABlock];
		nextTimer = 0;
	}
	return timers[nextTimer++];
}

static Uint32 timersAreFrozen = 0;

//
// Start the named timer.
//
void Timer::start(const char* name)
{
	if (timersAreFrozen)
		return;

	freezeTimers();

	TimerEntry& timer = getTimerEntry(name);
	PR_ASSERT(!timer.running);

	timer.accumulator = 0;
	timer.running = true;
	timer.done = false;

	unfreezeTimers();
}

//
// Stop the named timer.
//
void Timer::stop(const char* name)
{
	if (timersAreFrozen)
		return;

	freezeTimers();

	TimerEntry& timer = getTimerEntry(name);
	PR_ASSERT(timer.running);
	timer.running = false;
	timer.done = true;

	unfreezeTimers();
}

//
// Freeze all the running timers.
//
void Timer::freezeTimers()
{
	PRTime when = PR_Now() - adjust;

	if (timersAreFrozen == 0) {
		Vector<TimerEntry*> entries = timerEntries;
		Uint32 count = entries.size();

		for (Uint32 i = 0; i < count; i++) {
			TimerEntry& entry = *entries[i];
			if (entry.running) {
				entry.accumulator += (when - *entry.startTime);
			}
		}
	}
	timersAreFrozen++;
}
 
//
// Unfreeze all the running timers.
//
void Timer::unfreezeTimers()
{
	PR_ASSERT(timersAreFrozen != 0);
	timersAreFrozen--;

	if (timersAreFrozen == 0) {
		Vector<TimerEntry *> entries = timerEntries;
		Uint32 count = entries.size();
		
		PRTime& newStart = getNewTimer();
		
		for (Uint32 i = 0; i < count; i++) {
			TimerEntry& entry = *entries[i];
			if (entry.running) {
				entry.startTime = &newStart;
			}
		}
		
		newStart = PR_Now();
	}
}

//
// Print the named timer in the file f.
//
void Timer::print(FILE* f, const char *name)
{
	if (timersAreFrozen)
		return;

	freezeTimers();

	TimerEntry& timer = getTimerEntry(name);
  
	PR_ASSERT(timer.done);
	PRTime elapsed = timer.accumulator;

	if (elapsed >> 32) {
		fprintf(f, "[timer %s out of range]\n", name);
	} else {
		fprintf(f, "[%dus in %s]\n", Uint32(elapsed), name);
	}
	fflush(f);

	unfreezeTimers();
}

