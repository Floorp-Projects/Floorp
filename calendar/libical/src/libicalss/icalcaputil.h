/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalutil.h
 CREATOR: eric 23 December 1999


 $Id: icalcaputil.h,v 1.1.1.1 2001/01/02 07:33:03 ebusboom Exp $
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


/* Create new components that have a form suitable for the various
   iTIP trasactions */

/* Scheduling commands */ 
icalcomponent* icalcaputil_new_accept_reply(icalcomponent* comp, char* calid);
icalcomponent* icalcaputil_new_decline_reply(icalcomponent* comp, char* calid);
icalcomponent* icalcaputil_new_refresh(icalcomponent* comp, char* calid);
icalcomponent* icalcaputil_new_cancel(icalcomponent* comp);
icalcomponent* icalcaputil_new_counter(icalcomponent* comp);
icalcomponent* icalcaputil_new_declinecounter(icalcomponent* comp);

/* Calendaring commands */
icalcomponent* icalcaputil_new_create();
icalcomponent* icalcaputil_new_delete();
icalcomponent* icalcaputil_new_modify();
icalerrorenum* icalcaputil_modify_add_old_prop(icalcomponent* c, 
					       icalproperty *p);
icalerrorenum* icalcaputil_modify_add_new_prop(icalcomponent* c, 
					       icalproperty *p);
icalerrorenum* icalcaputil_add_query(icalcomponent* c, char* str);


icalcomponent* icalcaputil_new_move();
icalcomponent* icalcaputil_new_read();

icalerrorenum icalcaputil_add_target(icalcomponent* comp,char* target);
icalerrorenum icalcaputil_add_to_vcar(icalcomponent* comp,char* target);


				    


