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

#ifndef _ABNTFY_H_
#define _ABNTFY_H_

#include "abcom.h"
#include "ptrarray.h"

class AB_ContainerInfo;
class AB_ChangeListener;
class AB_ContainerListener;
class AB_PaneListener;

/*************************************************************************************************

  Our notifications are going to use an Announcer/Listener relationship. All announcers inherit from
  the AB_ChangeAnnouncer and all listeners inherit from the AB_ChangeListener classes. You should
  subclass your new notification classes appropriately. Currently, we have two flavors: 
  (1) container announcer/listeners and (2) pane announcer/listeners. 

  Container announcers are our containers! They typically announce changes to the database for a 
  particular container. Container listeners are usually objects who wish to be notified whenever 
  entries or attributes for that container change (or go away). In our model, typical container 
  listeners are the container pane, the AB_Pane, the mailing list pane and the person entry pane.
  You must specify if you wish to be a index of ABID based listener. Panes like the AB_Pane would
  typically be view index based while panes like the name completion pane are entry ID types.

  Pane announcers are going to be the different panes used in the address book. Pane listeners 
  want to be notified of changes to a pane's status (i.e. is the pane going away?). Pane listeners
  should not expect to be notified of database changes which may have altered the contents of the pane
  announcer. Use a container listener for this instead. A typical use for pane announcer/listeners 
  include the AB_Pane and the Person Entry pane. When we create a person entry pane from the AB_Pane
  (by issuing a AB_NewCard or AB_PropertiesCmd), the person entry pane becomes a pane listener on the
  AB_Pane. Why? Because if the AB_Pane is later closed, we need to annouce to the person entry pane
  that we are closing so that it can announce to the FE that it also needs closed! 

  Please, avoid multiple inheritence as it may not be supported cross platform. If you have an object 
  which you want to both be a pane and container listener, don't do it! Follow the example used with
  the person entry pane to see how an object can be two listeners without MI. 

  **************************************************************************************************/

typedef enum 
{
	AB_NotifyInserted,
	AB_NotifyDeleted,
	AB_NotifyPropertyChanged,

	AB_NotifyAll,		   /* contents of the have totally changed. Listener must totally
							  forget anything they knew about the object. */
	/* pane notifications (i.e. not tied to a particular entry */
	AB_NotifyScramble,     /* same contents, but the view indices have all changed 
						      i.e the object was sorted on a different attribute */
	AB_NotifyLDAPTotalContentChanged,
	AB_NotifyNewTopIndex,
	AB_NotifyStartSearching,
	AB_NotifyStopSearching

} AB_NOTIFY_CODE;


/*****************************************************************************************
  
	Declarations for the Change announcer/Listener base classes. You will need to subclass
	these two base classes if you want to add your own notification class. (see the 
	container and pane announcer / listeners as examples of how to do this). 

*****************************************************************************************/

class AB_ChangeListenerArray : public XPPtrArray
{
public:
	AB_ChangeListener * GetAt(int i) const { return (AB_ChangeListener *) XPPtrArray::GetAt(i);}
};

class AB_ChangeAnnouncer : public AB_ChangeListenerArray /* array of listeners */
{
public:
	AB_ChangeAnnouncer();
	~AB_ChangeAnnouncer();

	virtual XP_Bool AddListener(AB_ChangeListener * listener);
	virtual XP_Bool RemoveListener(AB_ChangeListener * listener);
	
	// notification methods should be defined in the appropriate sub classes
};

class AB_ChangeListener
{
public:
	AB_ChangeListener();
	virtual ~AB_ChangeListener();

	// notification methods should be defined in the appropriate sub classes
};

/*****************************************************************************************
  
	Declarations for the container announcer/listener classes

*****************************************************************************************/

class AB_ContainerAnnouncer : public AB_ChangeAnnouncer
{
public:
	AB_ContainerAnnouncer();
	virtual ~AB_ContainerAnnouncer();

	AB_ContainerListener * GetAt(int i) const { return (AB_ContainerListener *) XPPtrArray::GetAt(i);}

	// notification related routines. A container can have entries which can change OR the container itself could change.
	// we need notifications to support both of these. 
	void NotifyContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE, AB_ContainerListener * instigator);
	void NotifyAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);

	void NotifyContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE, MSG_ViewIndex index, ABID entryID,AB_ContainerListener * instigator);
	void NotifyContainerEntryRangeChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE, MSG_ViewIndex index, int32 numChanged, AB_ContainerListener * instigator);

};

class AB_ContainerListener : public AB_ChangeListener
{
public:
	AB_ContainerListener();
	virtual ~AB_ContainerListener();
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);

	virtual void OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, ABID /* entryID */, AB_ContainerListener * /* instigator */) = 0;
	virtual void OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, MSG_ViewIndex /* index */, int32 /* numChanged */, AB_ContainerListener * /* instigator */) = 0;

};

class AB_IndexBasedContainerListener : public AB_ContainerListener
{
public:
	AB_IndexBasedContainerListener();
	virtual ~AB_IndexBasedContainerListener();

	// notification related methods
	virtual void OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, MSG_ViewIndex index, int32 numChanged, AB_ContainerListener *instigator) = 0;
	/* index based listeners should ignore the call to entry ID changes */
	virtual void OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, ABID /* entryID */, AB_ContainerListener * /* instigator */) { return; }

}; 

class AB_ABIDBasedContainerListener : public AB_ContainerListener
{
public:
	AB_ABIDBasedContainerListener();
	virtual ~AB_ABIDBasedContainerListener();

	/* ABID listeners should ignore the call to view index changes changes */
	virtual void OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, MSG_ViewIndex /* index */, int32 /* numChanged */, AB_ContainerListener * /* instigator */) { return; }
	virtual void OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator) = 0;
};


/*****************************************************************************************
  
	Declarations for the pane announcer/listener classes

*****************************************************************************************/

class AB_PaneAnnouncer : public AB_ChangeAnnouncer
{
public:
	AB_PaneAnnouncer();
	~AB_PaneAnnouncer();

	AB_PaneListener * GetAt(int i) const { return (AB_PaneListener *) XPPtrArray::GetAt(i);}

	// notification related routines. Right now the only pane announcing we do is when we are going away...add more later
	// if we need them...
	void NotifyAnnouncerGoingAway(AB_PaneAnnouncer * instigator);
};

class AB_PaneListener : public AB_ChangeListener
{
public:
	AB_PaneListener();
	virtual ~AB_PaneListener();

	// notification related methods
	virtual void OnAnnouncerGoingAway(AB_PaneAnnouncer * instigator);
};

#endif
