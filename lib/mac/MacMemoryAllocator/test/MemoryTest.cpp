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


#include <iostream>


#if __profile__
#include <Profiler.h>
#include <Processes.h>
#endif

#include "CMemoryWorkout.h"

void main(void)
{

#if __profile__
	if (ProfilerInit(collectDetailed, bestTimeBase, 500, 20) != noErr)
		ExitToShell();
#endif

	// if you comment out the std::cout << calls below for profiling
	// reasons, you need to called InitGraf because I use Random()
	
	//::InitGraf(&qd.thePort);
	
	std::cout << "Starting up";

	CMemoryTester	*tester = new CMemoryTester;
	
	for (UInt32 i = 0; i < 1; i ++)
	{
	
		std::cout << i << std::endl;
		
		try
		{
			tester->DoMemoryTesting();
		}
		catch(...)
		{
			std::cout << "Caught exception";
		}
		
	}

	delete tester;

#if __profile__
	ProfilerDump("\pMemoryTester.prof");
	ProfilerTerm();
#endif
}

