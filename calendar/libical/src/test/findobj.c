/* -*- Mode: C -*-
  ======================================================================
  FILE: findobj.c
  CREATOR: eric 11 February 2000
  
  $Id: findobj.c,v 1.2 2002/06/28 10:45:12 acampi Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000 Eric Busboom
 http://www.softwarestudio.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/

#include <stdio.h> /* for printf */
#include <errno.h>
#include <string.h> /* For strerror */

#include "ical.h"
#include "icalss.h"

/* This program finds an object stored in a calendar */

void usage(char* arg0) {
    printf("usage: %s calendar-dir uid\n",arg0);
}

int main(int c, char *argv[]){

    icalcalendar *cal;
    icaldirset *booked;
    icalcomponent *itr;

    if(c < 2 || c > 3){
	usage(argv[0]);
	exit(1);
    }

    cal = icalcalendar_new(argv[1]);

    if(cal == 0){
	fprintf(stderr,"%s: error in opening calendar \"%s\": %s. errno is \"%s\"\n",
		argv[0],argv[1],icalerror_strerror(icalerrno),
		strerror(errno));
    }

    booked = icalcalendar_get_booked(cal);

    itr = icaldirset_fetch(booked,argv[2]);


    if(itr != 0){
	printf("%s",icalcomponent_as_ical_string(itr));
    }

    return 0;
}

