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
   NotificationCenter.cpp -- Callback mechanism for XFE
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */



#include "NotificationCenter.h"
#include "xp_mem.h"
#include "xp_str.h"
#include "xpassert.h"

XFE_NotificationCenter::XFE_NotificationCenter()
{
	m_hashtable = NULL;

	m_forwarder = NULL;
	m_numlists = 0;
}

XFE_NotificationCenter::~XFE_NotificationCenter()
{
	if (!m_hashtable)
		return;

	/* use the enumerator to step down and destroy all the callback lists. */
	PR_HashTableEnumerateEntries(m_hashtable,
								 (PRHashEnumerator)XFE_NotificationCenter::destroyHashEnumerator,
								 NULL);

	PR_HashTableDestroy(m_hashtable);
}

int
XFE_NotificationCenter::destroyHashEnumerator(PRHashEntry *he, int, void *)
{
	XFE_NotificationList *list = (XFE_NotificationList*)he->value;

	XP_FREE(list->notification_type);
	XP_FREE(list->callbacks);
	XP_FREE(list);

	return HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE;
}

XFE_NotificationList *
XFE_NotificationCenter::getNotificationListForName(const char *name)
{
	XFE_NotificationList *list = (XFE_NotificationList*)PR_HashTableLookup(m_hashtable, name);

	return list;
}

XFE_NotificationList *
XFE_NotificationCenter::addNewNotificationList(const char *name)
{
	XFE_NotificationList *new_list = XP_NEW_ZAP(XFE_NotificationList);

	new_list->notification_type = XP_STRDUP(name);
	new_list->num_interested = 0;
	new_list->num_alloced = 5; // start with 5 and realloc if necessary.

	new_list->callbacks = 
		(XFE_CallbackElement*)XP_CALLOC(new_list->num_alloced, 
										sizeof(XFE_CallbackElement));
	
	PR_HashTableAdd(m_hashtable, name, new_list);

	m_numlists ++;

	return new_list;
}

void 
XFE_NotificationCenter::registerInterest(const char *notification_name,
										 XFE_NotificationCenter *obj,
										 XFE_FunctionNotification notification_func,
										 void *clientData)
{
	XFE_NotificationList *list;

	if (!m_hashtable)
		m_hashtable = PR_NewHashTable(5, PR_HashString, PR_CompareStrings, PR_CompareValues, NULL, NULL);
	
	list = getNotificationListForName(notification_name);
	
	if (!list)
		list = addNewNotificationList(notification_name);
	
	if (list->num_alloced == list->num_interested)
		{
			list->num_alloced *= 2;
			
			list->callbacks = (XFE_CallbackElement*)XP_REALLOC(list->callbacks,
															   sizeof(XFE_CallbackElement) * list->num_alloced);
		}
	
	list->callbacks[ list->num_interested ].obj = obj;
	list->callbacks[ list->num_interested ].callbackFunction = notification_func;
	list->callbacks[ list->num_interested ].clientData = clientData;
	
	list->num_interested ++;
}

void 
XFE_NotificationCenter::unregisterInterest(const char *notification_name,
										   XFE_NotificationCenter *obj,
										   XFE_FunctionNotification notification_func,
										   void *clientData)
{
	int j,k;
	XFE_NotificationList *list;

	if (!m_hashtable)
		return;

	list = getNotificationListForName(notification_name);
	
	if (list)
		{
			for (j = 0; j < list->num_interested; j ++)
				{
					if ( list->callbacks[ j ].obj == obj
						 && list->callbacks[ j ].callbackFunction == notification_func
						 && list->callbacks[ j ].clientData == clientData )
						{
							for (k = j; k < list->num_interested - 1; k ++)
								list->callbacks[ k ] = list->callbacks[ k + 1 ];
							
							list->num_interested --;
							return;
						}
				}
		}
}

XP_Bool
XFE_NotificationCenter::hasInterested(const char *notification_center)
{
	if (!m_hashtable)
		{
			return FALSE;
		}
	else
		{
			XFE_NotificationList *list = getNotificationListForName(notification_center);
			return (list != NULL && list->num_interested >= 1);
		}
}

void
XFE_NotificationCenter::setForwarder(XFE_NotificationCenter *obj)
{
	m_forwarder = obj;
}

XFE_NotificationCenter *
XFE_NotificationCenter::getForwarder()
{
	return m_forwarder;
}

void 
XFE_NotificationCenter::notifyInterested(const char *notification_name,
					 void *callData)
{
	if (m_forwarder && m_forwarder != this)
		{
			m_forwarder->notifyInterested(notification_name, callData);
		}
	else
		{
			int j;
			XFE_NotificationList *list;
			
			if (!m_hashtable)
				return;

			list = getNotificationListForName(notification_name);
			
			if (list)
				{
					for (j = 0; j < list->num_interested; j ++)
						{
							XP_ASSERT(list->callbacks[j].callbackFunction);
							
							if (list->callbacks[j].callbackFunction)
								{
									(*list->callbacks[j].callbackFunction)
										(this, list->callbacks[j].obj, list->callbacks[j].clientData, callData);
								}
						}
				}
		}
}
  

