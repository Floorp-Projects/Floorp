/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmessage.h
 CREATOR: eric 07 Nov 2000


 $Id: icalmessage.h,v 1.1.1.1 2001/01/02 07:33:05 ebusboom Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


 =========================================================================*/

#include "ical.h"

#ifndef ICALMESSAGE_H
#define ICALMESSAGE_H


icalcomponent* icalmessage_new_accept_reply(icalcomponent* c, 
					    const char* user,
					    const char* msg);

icalcomponent* icalmessage_new_decline_reply(icalcomponent* c,
					    const char* user,
					    const char* msg);

/* New is modified version of old */
icalcomponent* icalmessage_new_counterpropose_reply(icalcomponent* oldc,
						    icalcomponent* newc,
						    const char* user,
						    const char* msg);


icalcomponent* icalmessage_new_delegate_reply(icalcomponent* c,
					      const char* user,
					      const char* delegatee,
					      const char* msg);


icalcomponent* icalmessage_new_cancel_event(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_instance(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_all(icalcomponent* c,
					    const char* user,
					    const char* msg);


icalcomponent* icalmessage_new_error_reply(icalcomponent* c,
					   const char* user,
					   const char* msg,
					   const char* debug,
					   icalrequeststatus rs);


#endif /* ICALMESSAGE_H*/
