/* -*- Mode: C -*-*/
/*======================================================================
 FILE: icalmime.c
 CREATOR: eric 26 July 2000


 $Id: icalmime.c,v 1.8 2005/01/24 12:49:11 acampi Exp $
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

#include "icalmime.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "sspm.h"
#include "stdlib.h"
#include <string.h> /* For strdup */
#include <stdio.h> /* for snprintf*/

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

/* These *_part routines are called by the MIME parser via the
   local_action_map */

struct text_part
{
	char* buf;
	char* buf_pos;
	size_t buf_size;
};

void* icalmime_text_new_part()
{

#define BUF_SIZE 2048

    struct text_part* impl;

    if ( ( impl = (struct text_part*)
	   malloc(sizeof(struct text_part))) == 0) {
	return 0;
    }

    impl->buf =  icalmemory_new_buffer(BUF_SIZE);
    impl->buf_pos = impl->buf;
    impl->buf_size = BUF_SIZE;

    return impl;    
}
void icalmime_text_add_line(void *part, 
			    struct sspm_header *header, 
			    char* line, size_t size)
{
    struct text_part* impl = (struct text_part*) part;

    icalmemory_append_string(&(impl->buf),&(impl->buf_pos),
			     &(impl->buf_size),line);
    
}

void* icalmime_textcalendar_end_part(void* part)
{

    struct text_part* impl = (struct text_part*) part;
    icalcomponent *c = icalparser_parse_string(impl->buf);

    icalmemory_free_buffer(impl->buf);
    free(impl);

    return c;

}

void* icalmime_text_end_part(void* part)
{
    struct text_part* impl = ( struct text_part*) part;

    icalmemory_add_tmp_buffer(impl->buf);
    free(impl);

    return impl->buf;
}

void icalmime_text_free_part(void *part)
{
    part = part;
}


/* Ignore Attachments for now */

void* icalmime_attachment_new_part()
{
    return 0;
}
void icalmime_attachment_add_line(void *part, struct sspm_header *header, 
				  char* line, size_t size)
{
    part = part;
    header = header;
    line = line;
    size = size;
}

void* icalmime_attachment_end_part(void* part)
{
    return 0;
}

void icalmime_attachment_free_part(void *part)
{
}




struct sspm_action_map icalmime_local_action_map[] = 
{
    {SSPM_TEXT_MAJOR_TYPE,SSPM_CALENDAR_MINOR_TYPE,icalmime_text_new_part,icalmime_text_add_line,icalmime_textcalendar_end_part,icalmime_text_free_part},
    {SSPM_TEXT_MAJOR_TYPE,SSPM_ANY_MINOR_TYPE,icalmime_text_new_part,icalmime_text_add_line,icalmime_text_end_part,icalmime_text_free_part},
    {SSPM_TEXT_MAJOR_TYPE,SSPM_PLAIN_MINOR_TYPE,icalmime_text_new_part,icalmime_text_add_line,icalmime_text_end_part,icalmime_text_free_part},
    {SSPM_APPLICATION_MAJOR_TYPE,SSPM_CALENDAR_MINOR_TYPE,icalmime_attachment_new_part,icalmime_attachment_add_line,icalmime_attachment_end_part,icalmime_attachment_free_part},
    {SSPM_IMAGE_MAJOR_TYPE,SSPM_CALENDAR_MINOR_TYPE,icalmime_attachment_new_part,icalmime_attachment_add_line,icalmime_attachment_end_part,icalmime_attachment_free_part},
    {SSPM_AUDIO_MAJOR_TYPE,SSPM_CALENDAR_MINOR_TYPE,icalmime_attachment_new_part,icalmime_attachment_add_line,icalmime_attachment_end_part,icalmime_attachment_free_part},
    {SSPM_IMAGE_MAJOR_TYPE,SSPM_CALENDAR_MINOR_TYPE,icalmime_attachment_new_part,icalmime_attachment_add_line,icalmime_attachment_end_part,icalmime_attachment_free_part},
   {SSPM_UNKNOWN_MAJOR_TYPE,SSPM_UNKNOWN_MINOR_TYPE,0,0,0,0}
};

#define NUM_PARTS 100 /* HACK. Hard Limit */



struct sspm_part* icalmime_make_part(icalcomponent* comp)
{
    comp = comp;
    return 0;
}

char* icalmime_as_mime_string(char* icalcomponent);

icalcomponent* icalmime_parse(char* (*get_string)(char *s, size_t size, 
						       void *d),
				void *data)
{
    struct sspm_part *parts;
    int i, last_level=0;
    icalcomponent *root=0, *parent=0, *comp=0, *last = 0;

    if ( (parts = (struct sspm_part *)
	  malloc(NUM_PARTS*sizeof(struct sspm_part)))==0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(parts,0,sizeof(parts));

    sspm_parse_mime(parts, 
		    NUM_PARTS, /* Max parts */
		    icalmime_local_action_map, /* Actions */ 
		    get_string,
		    data, /* data for get_string*/
		    0 /* First header */);



    for(i = 0; i <NUM_PARTS && parts[i].header.major != SSPM_NO_MAJOR_TYPE ; i++){

#define TMPSZ 1024
	char mimetype[TMPSZ];			       
	char* major = sspm_major_type_string(parts[i].header.major);
	char* minor = sspm_minor_type_string(parts[i].header.minor);

	if(parts[i].header.minor == SSPM_UNKNOWN_MINOR_TYPE ){
	    assert(parts[i].header.minor_text !=0);
	    minor = parts[i].header.minor_text;
	}
	
	snprintf(mimetype,sizeof(mimetype),"%s/%s",major,minor);

	comp = icalcomponent_new(ICAL_XLICMIMEPART_COMPONENT);

	if(comp == 0){
	    /* HACK Handle Error */
	    assert(0);
	}

	if(parts[i].header.error!=SSPM_NO_ERROR){
	    char *str = "Unknown error";
	    char temp[256];

	    if(parts[i].header.error==SSPM_MALFORMED_HEADER_ERROR){
		str = "Malformed header, possibly due to input not in MIME format";
	    }

	    if(parts[i].header.error==SSPM_UNEXPECTED_BOUNDARY_ERROR){
		str = "Got an unexpected boundary, possibly due to a MIME header for a MULTIPART part that is missing the Content-Type line";
	    }

	    if(parts[i].header.error==SSPM_WRONG_BOUNDARY_ERROR){
		str = "Got the wrong boundary for the opening of a MULTIPART part.";
	    }

	    if(parts[i].header.error==SSPM_NO_BOUNDARY_ERROR){
		str = "Got a multipart header that did not specify a boundary";
	    }

	    if(parts[i].header.error==SSPM_NO_HEADER_ERROR){
		str = "Did not get a header for the part. Is there a blank\
line between the header and the previous boundary\?";

	    }

	    if(parts[i].header.error_text != 0){
		snprintf(temp,sizeof(temp),
			 "%s: %s",str,parts[i].header.error_text);
	    } else {
		strcpy(temp,str);
	    }

	    icalcomponent_add_property
		(comp,
		 icalproperty_vanew_xlicerror(
		     temp,
		     icalparameter_new_xlicerrortype(
			 ICAL_XLICERRORTYPE_MIMEPARSEERROR),
		     0));  
	}

	if(parts[i].header.major != SSPM_NO_MAJOR_TYPE &&
	   parts[i].header.major != SSPM_UNKNOWN_MAJOR_TYPE){

	    icalcomponent_add_property(comp,
		icalproperty_new_xlicmimecontenttype((char*)
				icalmemory_strdup(mimetype)));

	}

	if (parts[i].header.encoding != SSPM_NO_ENCODING){

	    icalcomponent_add_property(comp,
	       icalproperty_new_xlicmimeencoding(
		   sspm_encoding_string(parts[i].header.encoding)));
	}

	if (parts[i].header.filename != 0){
	    icalcomponent_add_property(comp,
	       icalproperty_new_xlicmimefilename(parts[i].header.filename));
	}

	if (parts[i].header.content_id != 0){
	    icalcomponent_add_property(comp,
	       icalproperty_new_xlicmimecid(parts[i].header.content_id));
	}

	if (parts[i].header.charset != 0){
	    icalcomponent_add_property(comp,
	       icalproperty_new_xlicmimecharset(parts[i].header.charset));
	}

	/* Add iCal components as children of the component */
	if(parts[i].header.major == SSPM_TEXT_MAJOR_TYPE &&
	   parts[i].header.minor == SSPM_CALENDAR_MINOR_TYPE &&
	   parts[i].data != 0){

	    icalcomponent_add_component(comp,
					(icalcomponent*)parts[i].data);
	    parts[i].data = 0;

	} else 	if(parts[i].header.major == SSPM_TEXT_MAJOR_TYPE &&
	   parts[i].header.minor != SSPM_CALENDAR_MINOR_TYPE &&
	   parts[i].data != 0){

	    /* Add other text components as "DESCRIPTION" properties */

	    icalcomponent_add_property(comp,
               icalproperty_new_description(
		   (char*)icalmemory_strdup((char*)parts[i].data)));

	    parts[i].data = 0;
	}
	

	if(root!= 0 && parts[i].level == 0){
	    /* We've already assigned the root, but there is another
               part at the root level. This is probably a parse
               error*/
	    icalcomponent_free(comp);
	    continue;
	}

	if(parts[i].level == last_level && last_level != 0){
	    icalerror_assert(parent!=0,"No parent for adding component");

	    icalcomponent_add_component(parent,comp);

	} else if (parts[i].level == last_level && last_level == 0 &&
	    root == 0) {

	    root = comp;
	    parent = comp;

	} else if (parts[i].level > last_level){	    

	    parent = last;
	    icalcomponent_add_component(parent,comp);

	    last_level = parts[i].level;

	} else if (parts[i].level < last_level){

	    parent = icalcomponent_get_parent(parent);
	    icalcomponent_add_component(parent,comp);

	    last_level = parts[i].level;
	} else { 
	    assert(0);
	}

	last = comp;
	last_level = parts[i].level;
	assert(parts[i].data == 0);
    }

    sspm_free_parts(parts,NUM_PARTS);
    free(parts);

    return root;
}



int icalmime_test(char* (*get_string)(char *s, size_t size, void *d),
		  void *data)
{
    char *out;
    struct sspm_part *parts;
    int i;

    if ( (parts = (struct sspm_part *)
	  malloc(NUM_PARTS*sizeof(struct sspm_part)))==0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(parts,0,sizeof(parts));

    sspm_parse_mime(parts, 
		    NUM_PARTS, /* Max parts */
		    icalmime_local_action_map, /* Actions */ 
		    get_string,
		    data, /* data for get_string*/
		    0 /* First header */);

   for(i = 0; i <NUM_PARTS && parts[i].header.major != SSPM_NO_MAJOR_TYPE ; 
       i++){
       if(parts[i].header.minor == SSPM_CALENDAR_MINOR_TYPE){
	   parts[i].data = icalmemory_strdup(
	       icalcomponent_as_ical_string((icalcomponent*)parts[i].data));
       }
   }

    sspm_write_mime(parts,NUM_PARTS,&out,"To: bob@bob.org");

    printf("%s\n",out);

    return 0;

}


