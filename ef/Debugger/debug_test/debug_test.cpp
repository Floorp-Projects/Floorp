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

#include <iostream.h>
#include <stdio.h>
#include "nspr.h"
#include "debug_test.h"
#include "DebuggerChannel.h"

class 
DebuggerClientChannel*  gChannel;


void
DebuggerEvent::sendCommand(DebuggerClientChannel& inChannel)
{
	inChannel.sendOneCharacterRequest(cmd);
}

void
RemoteAgent::init()
{
	// Do nothing
}

void
RemoteAgent::SendEvent(DebuggerEvent& d)
{
	// Do socket stuff here

	cout << "Writing: " << d.cmd << endl;
	d.sendCommand(*gChannel);
}

#ifdef __GNUC__
// for gcc with -fhandle-exceptions
void terminate() {}
#endif

void
printMenu()
{
	cout << "b for breakpoint" << endl;
	cout << "g for go!" << endl;
	cout << "q for quit" << endl;
}

void 
sayLoadedMethod(const char* inName, void* inAddress)
{
	printf("Loaded/Compiled %s at %p\n", inName, inAddress);
}

void realMain(void*);

int
main(int argc, char *argv[])
{
	PRThread* thread;

	thread = PR_CreateThread(PR_USER_THREAD,
								realMain,
								NULL,
								PR_PRIORITY_NORMAL,
								PR_GLOBAL_THREAD,
								PR_JOINABLE_THREAD,
								0);

	PR_JoinThread(thread);
	return (0);
}

void realMain(void*)
{
	char c;

	if (!(gChannel = DebuggerClientChannel::createClient())) {
		fprintf(stderr, "failed to connect to local host on debugger port\n");
		exit(-1);
	}

	gChannel->setCompileOrLoadMethodHandler(sayLoadedMethod);

	while(1) {
		// print a menu of commands
		printMenu();

		// read a character

		cin >> c;
		cout << "read: " << c << endl;

		switch(c) {
			DebuggerEvent* de;

		case 'b':
			// Make up a breakpoint event
			de = new DebuggerEvent('b');
			RemoteAgent::SendEvent(*de);
			break;
		case 'g':
			// Make up a continue event
			de = new DebuggerEvent('g');
			RemoteAgent::SendEvent(*de);
			break;
		case 'q':
			// Make up a continue event
			de = new DebuggerEvent('q');
			RemoteAgent::SendEvent(*de);
			// Flag success
			exit(0);
			break;
		case 'a':
		{
			Int32 offset;
			char *methodName = gChannel->requestAddressToMethod((void*) 0x1623fa8, offset);
			if (methodName)
				printf("got Method: %s + %d\n", methodName, offset);
			else
				printf("unknown address\n");
			break;
		}
		case 'm':
		{
			void* address = gChannel->requestMethodToAddress("static private void java.lang.System.initializeSystemClass()");
			if (address)
				printf("got Method: %p\n", address);
			else
				printf("unknown method\n");
			break;
		}				
		case 'n':
			{
				const char* method = "public java.lang.Object javasoft.sqe.tests.api.java.lang.System.SystemTests11.check(suntest.quicktest.runtime.QT_Reporter,java.lang.Object[])";
				gChannel->requestNotifyOnMethodCompileLoad(method);
//"static private void java.lang.System.initializeSystemClass()");				
				break;
			}
		case 'r':
			{
				gChannel->requestRunClass("javasoft/sqe/tests/api/java/lang/System/SystemTests10");
			}
		default:
			cerr << "Unknown command" << endl;
			break;
		}
	}
}
