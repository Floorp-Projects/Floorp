/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmime.h
 CREATOR: eric 26 July 2000


 $Id: icalmime.h,v 1.1.1.1 2001/01/02 07:33:02 ebusboom Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

======================================================================*/

#ifndef ICALMIME_H
#define ICALMIME_H

#include "icalcomponent.h"
#include "icalparser.h"

icalcomponent* icalmime_parse(	char* (*line_gen_func)(char *s, size_t size, 
						       void *d),
				void *data);

/* The inverse of icalmime_parse, not implemented yet. Use sspm.h directly.  */
char* icalmime_as_mime_string(char* component);



#endif /* !ICALMIME_H */



