/* -*- Mode: C -*-
  ======================================================================
  FILE: icalfilesetimpl.h
  CREATOR: eric 23 December 1999
  
  $Id: icalclusterimpl.h,v 1.1 2002/05/29 12:38:31 acampi Exp $
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* This definition is in its own file so it can be kept out of the
   main header file, but used by "friend classes" like icaldirset*/

#define ICALCLUSTER_ID "clus"

struct icalcluster_impl {

	char		id[5];		/* clus */

	char	       *key;
	icalcomponent  *data;
	int		changed;
};
