/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcluster.h
 CREATOR: eric 23 December 1999


 $Id: icalcluster.h,v 1.2 2002/06/27 02:30:58 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALCLUSTER_H
#define ICALCLUSTER_H

#include "ical.h"
#include "icalset.h"

typedef struct icalcluster_impl icalcluster;

icalcluster* icalcluster_new(const char *key, icalcomponent *data);
icalcluster* icalcluster_new_clone(const icalcluster *cluster);

void icalcluster_free(icalcluster *cluster);

const char* icalcluster_key(icalcluster *cluster);
int icalcluster_is_changed(icalcluster *cluster);
void icalcluster_mark(icalcluster *cluster);
void icalcluster_commit(icalcluster *cluster);

icalcomponent* icalcluster_get_component(icalcluster* cluster);
int icalcluster_count_components(icalcluster *cluster, icalcomponent_kind kind);
icalerrorenum icalcluster_add_component(icalcluster* cluster,
					icalcomponent* child);
icalerrorenum icalcluster_remove_component(icalcluster* cluster,
					   icalcomponent* child);

icalcomponent* icalcluster_get_current_component(icalcluster* cluster);
icalcomponent* icalcluster_get_first_component(icalcluster* cluster);
icalcomponent* icalcluster_get_next_component(icalcluster* cluster);

#endif /* !ICALCLUSTER_H */



