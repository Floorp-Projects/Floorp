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
/* 
   NotificationCenter.h -- class definition for NotificationCenters
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_notificationcenter_h
#define _xfe_notificationcenter_h


#include "plhash.h"

#include "xp_core.h"

class XFE_NotificationCenter; /* must be defined for the callback stuff. */

typedef void (*XFE_FunctionNotification)(XFE_NotificationCenter *n_obj,
					 XFE_NotificationCenter *obj,
					 void *clientData,
					 void *callData);

// These macros are needed because some non GNU/SGI compilers didn't
// like us throwing method pointers around.
#define XFE_CALLBACK_DECL(name) \
	static void name##_cb(XFE_NotificationCenter *n_obj, XFE_NotificationCenter *obj, void *clientData, void *callData); \
	virtual void name(XFE_NotificationCenter *n_obj, void *clientData, void *callData);

#define XFE_CALLBACK_DEFN(classname, name) \
void classname::name##_cb(XFE_NotificationCenter *n_obj, XFE_NotificationCenter *obj, void *clientData, void *callData) \
{ \
  ((classname*)obj)->name(n_obj, clientData, callData); \
} \
void classname::name

typedef struct {
  XFE_NotificationCenter *obj;
  XFE_FunctionNotification callbackFunction;
  void *clientData;
} XFE_CallbackElement;

typedef struct {
  char *notification_type;
  
  int num_interested;
  int num_alloced;
  XFE_CallbackElement *callbacks;
} XFE_NotificationList;

// Notification Mechanism class.
// Subclass from here if you want this functionality.
//
// If object A wants to be notified when event e happens
// for object B, A needs to ask B to be put on the notification
// list for event e:
//
//   B->registerInterest(e,   // const char*
//                       A,   
//                       A::<callback>,
//                       <clientData>);
//
// then when the event e happens for B, B broadcasts this message:
//
//   B->notifyInterested(e);
//
// and A recieves the message in the form of a call to <callback>
// with <clientData>.  <clientData> is NULL by default.
// <callback> needs to be declared and implemented in object A:
//
//   // Public member function in A.h
//   XFE_CALLBACK_DECL(e)
//
//   // Anywhere in A.cpp:
//   XFE_CALLBACK_DEFN(A, e)(XFE_NotificationCenter */*obj*/, 
//                           void */*clientData*/, 
//                           void *callData)
//
// The event e is represented by a const char* stored in:
//   1) object B, or 
//   2) a third-party object that knows about A & B
//      if A & B don't know about each other, or
//   3) the superclass shared by A & B if there
//      is no third-party object that knows about A & B.
//
// Note that object A does not need to do the registering, a third-party
// object can do this (e.g. a XFE_Frame instance can set up a notification
// between the menubar and the dashboard since it knows about both).   CCM
class XFE_NotificationCenter
{
public:
	XFE_NotificationCenter();
	~XFE_NotificationCenter();
	
	// These two deal with static member function callbacks
	void registerInterest(const char *notification_name,
						  XFE_NotificationCenter *obj,
						  XFE_FunctionNotification notification_func,
						  void *clientData = NULL);
	
	void unregisterInterest(const char *notification_name,
							XFE_NotificationCenter *obj,
							XFE_FunctionNotification notification_func,
							void *clientData = NULL);
	
	void removeAllInterest(void); // removes everyone's registered interest.
	void removeAllInterest(XFE_NotificationCenter *obj); // remove all of obj's interest

	/* whether or not there is anyone that has registered interest in this
	   notification */
	XP_Bool hasInterested(const char *notification_name);

	/* This next method really should be protected, since 
	   we only want the objects themselves to invoke their
	   own callbacks.  But, I for one don't want to have 
	   to write upteen million XtCallback pairs (one static,
	   one non-static) to invoke a callback, so we expose this
	   function. */
	// Notify those that are interested
	void notifyInterested(const char *notification_name,
						  void *callData = NULL);
	
	void setForwarder(XFE_NotificationCenter *obj);
	XFE_NotificationCenter *getForwarder();
	
private:
	XFE_NotificationCenter *m_forwarder;
	
	PRHashTable *m_hashtable;
	
	int m_numlists;
	
	XFE_NotificationList *getNotificationListForName(const char *name);
	XFE_NotificationList *addNewNotificationList(const char *name);

	static int destroyHashEnumerator(PRHashEntry *he, int i, void *arg);
};

#endif /* _xfe_notificationcenter_h */

