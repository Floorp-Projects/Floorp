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

// InterestingEvents.h
//
// Scott M. Silver

// Getting notifed for and broadcasting interesting events in the
// system.

#ifndef _H_INTERESTINGEVENTS
#define _H_INTERESTINGEVENTS

#include "Vector.h"
#include "Fundamentals.h"
	
// Creating a new simple listener.
//
// First declare the event handler.
// name, the global event broadcaster you want (listed below)
//
// DECLARE_SIMPLE_EVENT_HANDLER(PrintCompilingMessage, gCompileBroadcaster)
//
// Now define the event handler
// name, parameter name for the MessageKind, and parameter name for the void* argument
//
// DEFINE_SIMPLE_EVENT_HANDLER(PrintCompilingMessage, inKind, inArgument)
// {
//		inKind; inArgument;		// unused parameters
//
//		printf("We're compiling something\n");
// }
//
// What is passed is defined by each broadcaster. (documented below)
//

// Creating a new broadcaster
//
// add below 
//	extern EventBroadcaster*	gMyBroadcaster; // (notice the *)
//
// add to InterestingEvents.cpp (or maybe in your file)
//	EventBroadcaster*	gMyBroadcaster;
// 
// For more compilcated listeners subclass EventListener
// and override the listenToMessage method.

enum BroadcastEventKind
{
	kBroacastCompile,
	kEndCompileOrLoad
};

// EventBroadcaster
//
// Maintains a vector of EventListeners (which currently cannot)
// be removed.  Each event listener is broadcast a message.
//
// Because of static-initializer ordering not crossing file
// boundaries we pass in a pointer to the broadcaster in each
// public function, which checks to see that the broadcaster
// was initialized.

class EventListener;
class EventBroadcaster
{
	Vector<EventListener*>	mListeners;
	void	addListener(EventListener&	inListener) { mListeners.append(&inListener); }
	void	removeListener(EventListener& /*inListener*/) { }
	void	broadcastEvent(BroadcastEventKind inKind, void* inArgument);

	static EventBroadcaster&	checkAllocated(EventBroadcaster*& ioBroadcaster)
	{
		if (!ioBroadcaster)
			ioBroadcaster = new EventBroadcaster();

		return *ioBroadcaster;
	}
	
public:
	static  void addListener(EventBroadcaster*& ioBroadcaster, EventListener& inListener)
	{
		checkAllocated(ioBroadcaster).addListener(inListener);
	}

	static  void broadcastEvent(EventBroadcaster*& ioBroadcaster, BroadcastEventKind inKind, void* inArgument)
	{
		checkAllocated(ioBroadcaster).broadcastEvent(inKind, inArgument);
	}

	static  void removeListener(EventBroadcaster*& ioBroadcaster, EventListener& inListener)
	{
		checkAllocated(ioBroadcaster).removeListener(inListener);
	}
};


// EventListener
//
// For listenToMessage is called each time a message is broadcast
//
class NS_EXTERN EventListener
{
	DEBUG_ONLY(Vector<EventBroadcaster*>	mBroadcasters);

public:
					EventListener(EventBroadcaster*& ioBroadcaster)
					{
						ioBroadcaster->addListener(ioBroadcaster, *this);
					}


					~EventListener() {  }

	virtual void	listenToMessage(BroadcastEventKind inKind, void* inArgument) = 0;
};


// Add new declarations of new broadcasters here.
extern EventBroadcaster* gCompileBroadcaster;
extern EventBroadcaster* gCompileOrLoadBroadcaster;

// Some helper macros described above
#define SIMPLE_EVENT_HANDLER_CLASS_NAME(inName)	\
		inName##Listener

#define DECLARE_SIMPLE_EVENT_HANDLER(inName, inBroadcaster)								\
	class SIMPLE_EVENT_HANDLER_CLASS_NAME(inName) :										\
		public EventListener															\
	{																					\
		public:																			\
			SIMPLE_EVENT_HANDLER_CLASS_NAME(inName)(EventBroadcaster*& ioListener) :	\
				EventListener(ioListener) {}											\
																						\
		void	listenToMessage(BroadcastEventKind inKind, void* inArgument);			\
		private:																		\
			static SIMPLE_EVENT_HANDLER_CLASS_NAME(inName)	s##inName##Listener;		\
	};																					\
																						\
	SIMPLE_EVENT_HANDLER_CLASS_NAME(inName) SIMPLE_EVENT_HANDLER_CLASS_NAME(inName)::s##inName##Listener(inBroadcaster);

#define DEFINE_SIMPLE_EVENT_HANDLER(inName, inMessageKindParamName, inArgumentParamName)		\
		void SIMPLE_EVENT_HANDLER_CLASS_NAME(inName)::											\
		listenToMessage(BroadcastEventKind inMessageKindParamName, void* inArgumentParamName)

#endif // _H_INTERESTINGEVENTS
