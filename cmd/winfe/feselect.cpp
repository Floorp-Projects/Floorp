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
 
#include "stdafx.h"

#include "feselect.h"
#include "timer.h"
#include "ssl.h"
#include "hiddenfr.h"

//  We have one and only one select tracker.
CSelectTracker selecttracker;


#define MAX_SIMULTANIOUS_SOCKETS 100
/* should never have more than MAX sockets */
PRPollDesc poll_desc_array[MAX_SIMULTANIOUS_SOCKETS];
unsigned int fd_set_size = 0;               

//  Have any entries in select list?
BOOL CSelectTracker::HasSelect(SelectType stType)
{
	return fd_set_size;
}

//  Number of select entries in list.
int CSelectTracker::CountSelect(SelectType stType)
{
    return(fd_set_size);
}

//  Add a select entry, no duplicates.
void CSelectTracker::AddSelect(SelectType stType, PRFileDesc *prFD)
{
	unsigned int index = fd_set_size;
	unsigned int count;

#define CONNECT_FLAGS PR_POLL_READ | PR_POLL_EXCEPT | PR_POLL_WRITE
#define READ_FLAGS    PR_POLL_READ | PR_POLL_EXCEPT

    //  Go through the list, make sure that there is not already an entry for fd.
	for(count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].fd == prFD)
		{
			// found it
			// verify that it has the same flags that we wan't, 
			// otherwise add it again with different flags
			if((stType == ConnectSelect && poll_desc_array[count].in_flags == CONNECT_FLAGS)
			   || (stType == SocketSelect && poll_desc_array[count].in_flags == READ_FLAGS))
			{
				index = count;
				break;
			}
		}
	}

	// need to add a new one if we didnt' find the index
	if(index == fd_set_size)
	{
		poll_desc_array[fd_set_size].fd = prFD;
		fd_set_size++;
	}
	
	if(stType == ConnectSelect)
		poll_desc_array[index].in_flags = CONNECT_FLAGS;
	else if(stType == SocketSelect)
		poll_desc_array[index].in_flags = READ_FLAGS;
	else
		XP_ASSERT(0);
}

//  Remove a select if it exists.
void CSelectTracker::RemoveSelect(SelectType stType, PRFileDesc *prFD)
{
	unsigned int count;

    //  Go through the list
	for(count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].fd == prFD)
		{
			
			if((stType == ConnectSelect && poll_desc_array[count].in_flags == CONNECT_FLAGS)
			   || (stType == SocketSelect && poll_desc_array[count].in_flags == READ_FLAGS))
			{
				// found it collapse the list

				fd_set_size--;
				if(count < fd_set_size)
					XP_MEMCPY(&poll_desc_array[count], &poll_desc_array[count+1], (fd_set_size - count) * sizeof(PRPollDesc));

				return;
			}
		}
	}

	// didn't find it.  opps
}

BOOL CSelectTracker::HasSelect(SelectType stType, PRFileDesc *prFD)
{
	unsigned int count;

    //  Go through the list
	for(count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].fd == prFD)
		{
			
			if((stType == ConnectSelect && poll_desc_array[count].in_flags == CONNECT_FLAGS)
			   || (stType == SocketSelect && poll_desc_array[count].in_flags == READ_FLAGS))
			   return TRUE;
		}
	}
	return FALSE;
}

extern "C"
void FE_AbortDNSLookup(PRFileDesc *fd)
{

}

extern "C"
void FE_SetReadPoll(PRFileDesc *fd) 
{
    selecttracker.AddSelect(SocketSelect, fd);
}

extern "C"
void FE_ClearReadPoll(PRFileDesc *fd) 
{
    selecttracker.RemoveSelect(SocketSelect, fd);
}

extern "C"
void FE_SetConnectPoll(PRFileDesc *fd) 
{
    selecttracker.AddSelect(ConnectSelect, fd);
}

extern "C"
void FE_ClearConnectPoll(PRFileDesc *fd) 
{
    selecttracker.RemoveSelect(ConnectSelect, fd);
}

/* call PR_Poll and call Netlib if necessary 
 *
 * return FALSE if nothing to do.
 */
Bool CSelectTracker::PollNetlib(void)
{
	static PRIntervalTime interval = PR_MillisecondsToInterval(1); 

	if(fd_set_size < 1)
		return FALSE;

	register int status = PR_Poll(poll_desc_array, fd_set_size, interval);

	if(status < 1)
		return TRUE; // potential for doing stuff in the future.

	// for now call on all active sockets.
	// if this is too much call only one, but reorder the list each time.
	for(register unsigned int count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].out_flags & (PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT))
			NET_ProcessNet(poll_desc_array[count].fd, NET_SOCKET_FD);
	}

	return TRUE;
}
