/* -*- Mode: C -*-
  ======================================================================
  FILE: icaldirsetimpl.h
  CREATOR: eric 21 Aug 2000
  
  $Id: icaldirsetimpl.h,v 1.4 2002/06/27 02:30:58 acampi Exp $
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

#include "icalcluster.h"

/* This definition is in its own file so it can be kept out of the
   main header file, but used by "friend classes" like icalset*/

struct icaldirset_impl 
{
  icalset super;		/**< parent class */
  char* dir;			/**< directory containing ics files  */
  icaldirset_options options;	/**< copy of options passed to icalset_new() */
  icalcluster* cluster;		/**< cluster containing data */
  icalgauge* gauge;		/**< gauge for filtering out data  */
  int first_component;		/**< ??? */
  pvl_list directory;		/**< ??? */
  pvl_elem directory_iterator;	/**< ??? */
};
