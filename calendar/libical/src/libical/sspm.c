/* -*- Mode: C -*-
  ======================================================================
  FILE: sspm.c Parse Mime
  CREATOR: eric 25 June 2000
  
  $Id: sspm.c,v 1.9 2005/01/24 13:15:19 acampi Exp $
  $Locker:  $
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/

#include <stdio.h>
#include <string.h>
#include "sspm.h"
#include <assert.h>
#include <ctype.h> /* for tolower */
#include <stdlib.h>   /* for malloc, free */
#include <string.h> /* for strcasecmp */

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#define TMP_BUF_SIZE 1024


enum mime_state {
    UNKNOWN_STATE,
    IN_HEADER,
    END_OF_HEADER,
    IN_BODY,
    OPENING_PART,
    END_OF_PART,
    TERMINAL_END_OF_PART,
    END_OF_INPUT
};

struct mime_impl{
	struct sspm_part *parts; 
	size_t max_parts;
	int part_no;
	int level;
	struct sspm_action_map *actions;
	char* (*get_string)(char *s, size_t size, void* data);
	void* get_string_data;
	char temp[TMP_BUF_SIZE];
	enum mime_state state;
};

void sspm_free_header(struct sspm_header *header);
void* sspm_make_multipart_part(struct mime_impl *impl,struct sspm_header *header);
void sspm_read_header(struct mime_impl *impl,struct sspm_header *header);

char* sspm_strdup(char* str){

    char* s;

    s = strdup(str);

    return s;
}


static struct  major_content_type_map 
{
	enum sspm_major_type type;
	char* str;
	
} major_content_type_map[]  = 
{
    {SSPM_MULTIPART_MAJOR_TYPE,"multipart" },
    {SSPM_TEXT_MAJOR_TYPE,"text" },
    {SSPM_TEXT_MAJOR_TYPE,"text" },
    {SSPM_IMAGE_MAJOR_TYPE,"image" },
    {SSPM_AUDIO_MAJOR_TYPE,"audio" },
    {SSPM_VIDEO_MAJOR_TYPE,"video" },
    {SSPM_APPLICATION_MAJOR_TYPE,"application" },
    {SSPM_MULTIPART_MAJOR_TYPE,"multipart" },
    {SSPM_MESSAGE_MAJOR_TYPE,"message" },
    {SSPM_UNKNOWN_MAJOR_TYPE,"" },
};

static struct  minor_content_type_map 
{
	enum sspm_minor_type type;
	char* str;

} minor_content_type_map[]  = 
{
    {SSPM_ANY_MINOR_TYPE,"*" },
    {SSPM_PLAIN_MINOR_TYPE,"plain" },
    {SSPM_RFC822_MINOR_TYPE,"rfc822" },
    {SSPM_DIGEST_MINOR_TYPE,"digest" },
    {SSPM_CALENDAR_MINOR_TYPE,"calendar" },
    {SSPM_MIXED_MINOR_TYPE,"mixed" },
    {SSPM_RELATED_MINOR_TYPE,"related" },
    {SSPM_ALTERNATIVE_MINOR_TYPE,"alternative" },
    {SSPM_PARALLEL_MINOR_TYPE,  "parallel" },  
    {SSPM_UNKNOWN_MINOR_TYPE,"" } 
};



struct encoding_map {
	enum sspm_encoding encoding;
	char* str;
} sspm_encoding_map[] = 
{
    {SSPM_NO_ENCODING,""},
    {SSPM_QUOTED_PRINTABLE_ENCODING,"quoted-printable"},
    {SSPM_8BIT_ENCODING,"8bit"},
    {SSPM_7BIT_ENCODING,"7bit"},
    {SSPM_BINARY_ENCODING,"binary"},
    {SSPM_BASE64_ENCODING,"base64"},
    {SSPM_UNKNOWN_ENCODING,""}

};


char* sspm_get_parameter(char* line, char* parameter)
{
    char *p,*s,*q;
    static char name[1024];
    
    /* Find where the parameter name is in the line */
    p = strstr(line,parameter);

    if( p == 0){
	return 0;
    }

    /* skip over the parameter name, the '=' and any blank spaces */

    p+=strlen(parameter);

    while(*p==' ' || *p == '='){
	p++;
    }

    /*now find the next semicolon*/

    s = strchr(p,';');

    /* Strip of leading quote */
    q = strchr(p,'\"');

    if(q !=0){
	p = q+1;
    }

    if(s != 0){
	strncpy(name,p,(size_t)s-(size_t)p);
    } else {
	strcpy(name,p);
    }

    /* Strip off trailing quote, if it exists */

    q = strrchr(name,'\"');

    if (q != 0){
	*q='\0';
    }
    
    return name;
}

char* sspm_property_name(char* line)
{
    static char name[1024];
    char *c = strchr(line,':');

    if(c != 0){
	strncpy(name,line,(size_t)c-(size_t)line);
	name[(size_t)c-(size_t)line] = '\0';
	return name;
    } else {
	return 0;
    }
}

char* sspm_value(char* line)
{
    static char value[1024];

    char *c,*s, *p;

    /* Find the first colon and the next semicolon */

    c = strchr(line,':');
    s = strchr(c,';');

    /* Skip the colon */
    c++;

    if (s == 0){
	s = c+strlen(line);
    }

    for(p=value; c != s; c++){
	if(*c!=' ' && *c!='\n'){
	    *(p++) = *c;
	}
    }

    *p='\0';

    return value;

}

static char *mime_headers[] = {
    "Content-Type",
    "Content-Transfer-Encoding",
    "Content-Disposition",
    "Content-Id",
    "Mime-Version",
    0 
};


void* sspm_default_new_part()
{
    return 0;
}
void sspm_default_add_line(void *part, struct sspm_header *header, 
			   char* line, size_t size)
{
}

void* sspm_default_end_part(void* part)
{
    return 0;
}

void sspm_default_free_part(void *part)
{
}



struct sspm_action_map sspm_action_map[] = 
{
    {SSPM_UNKNOWN_MAJOR_TYPE,SSPM_UNKNOWN_MINOR_TYPE,sspm_default_new_part,sspm_default_add_line,sspm_default_end_part,sspm_default_free_part},
};

int sspm_is_mime_header(char *line)
{
    char *name = sspm_property_name(line);
    int i;

    if(name == 0){ 
	return 0;
    }

    for(i = 0; mime_headers[i] != 0; i++){
	if(strcasecmp(name, mime_headers[i]) == 0)
	    return 1;
    }
    
    return 0;
}

int sspm_is_mail_header(char* line)
{
    char *name = sspm_property_name(line);

    if (name != 0){
	return 1;
    }

    return 0;

}

int sspm_is_blank(char* line)
{
    char *p;
    char c =0;

    for(p=line; *p!=0; p++){
	if( ! (*p == ' '|| *p == '\t' || *p=='\n') ){
	    c++;
	}
    }

    if (c==0){
	return 1;
    }

    return 0;
    
}

int sspm_is_continuation_line(char* line)
{
    if (line[0] == ' '|| line[0] == '\t' ) {
	return 1;
    }

    return 0;
}

int sspm_is_mime_boundary(char *line)
{
    if( line[0] == '-' && line[1] == '-') {
	return 1;
    } 

    return 0;
}

int sspm_is_mime_terminating_boundary(char *line)
{


    if (sspm_is_mime_boundary(line) &&
	strstr(line,"--\n")){
	return 1;
    } 

    return 0;
}

enum line_type {
    EMPTY,
    BLANK,
    MIME_HEADER,
    MAIL_HEADER,
    HEADER_CONTINUATION,
    BOUNDARY,
    TERMINATING_BOUNDARY,
    UNKNOWN_TYPE
};


static enum line_type get_line_type(char* line){

    if (line == 0){
	return EMPTY;
    } else if(sspm_is_blank(line)){
	return BLANK;
    } else if (sspm_is_mime_header(line)){
	return MIME_HEADER;
    } else if (sspm_is_mail_header(line)){
	return MAIL_HEADER;
    } else if (sspm_is_continuation_line(line)){
	return HEADER_CONTINUATION;
    } else if (sspm_is_mime_terminating_boundary(line)){
	return TERMINATING_BOUNDARY;
    } else if (sspm_is_mime_boundary(line)) {
	return BOUNDARY;
    } else {
	return UNKNOWN_TYPE;
    }


}


static struct sspm_action_map get_action(struct mime_impl *impl,
				  enum sspm_major_type major,
				  enum sspm_minor_type minor)
{
    int i;

    /* Read caller suppled action map */

    if (impl->actions != 0){
	for(i=0; impl->actions[i].major != SSPM_UNKNOWN_MAJOR_TYPE; i++){
	    if((major == impl->actions[i].major &&
	       minor == impl->actions[i].minor) ||
	       (major == impl->actions[i].major &&
		minor == SSPM_ANY_MINOR_TYPE)){
		return impl->actions[i];
	    }
	}
    }

    /* Else, read default action map */

    for(i=0; sspm_action_map[i].major != SSPM_UNKNOWN_MAJOR_TYPE; i++){
	    if((major == sspm_action_map[i].major &&
	       minor == sspm_action_map[i].minor) ||
	       (major == sspm_action_map[i].major &&
		minor == SSPM_ANY_MINOR_TYPE)){
	    break;
	}
    }
    
    return sspm_action_map[i];
}


char* sspm_lowercase(char* str)
{
    char* p = 0;
    char* ret;

    if(str ==0){
	return 0;
    }

    ret = sspm_strdup(str);

    for(p = ret; *p!=0; p++){
	*p = tolower(*p);
    }

    return ret;
}

enum sspm_major_type sspm_find_major_content_type(char* type)
{
    int i;

    char* ltype = sspm_lowercase(type);

    for (i=0; major_content_type_map[i].type !=  SSPM_UNKNOWN_MINOR_TYPE; i++){
	if(strncmp(ltype, major_content_type_map[i].str,
		   strlen(major_content_type_map[i].str))==0){
	    free(ltype);
	    return major_content_type_map[i].type;
	}
    }
    free(ltype);
    return major_content_type_map[i].type; /* Should return SSPM_UNKNOWN_MINOR_TYPE */
}

enum sspm_minor_type sspm_find_minor_content_type(char* type)
{
    int i;
    char* ltype = sspm_lowercase(type);

    char *p = strchr(ltype,'/');

    if (p==0){
	return SSPM_UNKNOWN_MINOR_TYPE; 
    }

    p++; /* Skip the '/' */

    for (i=0; minor_content_type_map[i].type !=  SSPM_UNKNOWN_MINOR_TYPE; i++){
	if(strncmp(p, minor_content_type_map[i].str,
		   strlen(minor_content_type_map[i].str))==0){
	    free(ltype);
	    return minor_content_type_map[i].type;
	}
    }
    
    free(ltype);
    return minor_content_type_map[i].type; /* Should return SSPM_UNKNOWN_MINOR_TYPE */
}

char* sspm_major_type_string(enum sspm_major_type type)
{
    int i;

    for (i=0; major_content_type_map[i].type !=  SSPM_UNKNOWN_MINOR_TYPE; 
	 i++){

	if(type == major_content_type_map[i].type){
	    return major_content_type_map[i].str;
	}
    }
    
    return major_content_type_map[i].str; /* Should return SSPM_UNKNOWN_MINOR_TYPE */
}

char* sspm_minor_type_string(enum sspm_minor_type type)
{
    int i;
    for (i=0; minor_content_type_map[i].type !=  SSPM_UNKNOWN_MINOR_TYPE; 
	 i++){
	if(type == minor_content_type_map[i].type){
	    return minor_content_type_map[i].str;
	}
    }
    
    return minor_content_type_map[i].str; /* Should return SSPM_UNKNOWN_MINOR_TYPE */
}


char* sspm_encoding_string(enum sspm_encoding type)
{
    int i;
    for (i=0; sspm_encoding_map[i].encoding !=  SSPM_UNKNOWN_ENCODING; 
	 i++){
	if(type == sspm_encoding_map[i].encoding){
	    return sspm_encoding_map[i].str;
	}
    }
    
    return sspm_encoding_map[i].str; /* Should return SSPM_UNKNOWN_MINOR_TYPE */
}

/* Interpret a header line and add its data to the header
   structure. */
void sspm_build_header(struct sspm_header *header, char* line)
{
    char *prop;
    char *val;
    
    val = sspm_strdup(sspm_value(line));
    prop = sspm_strdup(sspm_property_name(line));

    if(strcmp(prop,"Content-Type") == 0){
	
	/* Create a new mime_header, fill in content-type
	   and possibly boundary */
	
	char* boundary= sspm_get_parameter(line,"boundary");
	
	header->def = 0;
	header->major = sspm_find_major_content_type(val);
	header->minor = sspm_find_minor_content_type(val);
	
	if(header->minor == SSPM_UNKNOWN_MINOR_TYPE){
	    char *p = strchr(val,'/');
	    
	    if (p != 0){
		p++; /* Skip the '/' */
		
		header->minor_text = sspm_strdup(p);
	    } else {
		/* Error, malformed content type */
		header->minor_text = sspm_strdup("unknown");
	    }
	}
	if (boundary != 0){
	    header->boundary = sspm_strdup(boundary);
	}
	
    } else if(strcmp(prop,"Content-Transfer-Encoding")==0){
	char* encoding = sspm_value(line);
	char* lencoding = sspm_lowercase(encoding);

	if(strcmp(lencoding,"base64")==0){
	    header->encoding = SSPM_BASE64_ENCODING;
	} else 	if(strcmp(lencoding,"quoted-printable")==0){
	    header->encoding = SSPM_QUOTED_PRINTABLE_ENCODING;
	} else 	if(strcmp(lencoding,"binary")==0){
	    header->encoding = SSPM_BINARY_ENCODING;
	} else 	if(strcmp(lencoding,"7bit")==0){
	    header->encoding = SSPM_7BIT_ENCODING;
	} else 	if(strcmp(lencoding,"8bit")==0){
	    header->encoding = SSPM_8BIT_ENCODING;
	} else {
	    header->encoding = SSPM_UNKNOWN_ENCODING;
	}


	free(lencoding);

	header->def = 0;
	
    } else if(strcmp(prop,"Content-Id")==0){
	char* cid = sspm_value(line);
	header->content_id = sspm_strdup(cid);
	header->def = 0;
	
    }
    free(val);
    free(prop);
}

char* sspm_get_next_line(struct mime_impl *impl)
{
    char* s;
    s = impl->get_string(impl->temp,TMP_BUF_SIZE,impl->get_string_data);
    
    if(s == 0){
	impl->state = END_OF_INPUT;
    }
    return s;
}


void sspm_store_part(struct mime_impl *impl, struct sspm_header header,
		      int level, void *part, size_t size)
{
    
    impl->parts[impl->part_no].header = header;
    impl->parts[impl->part_no].level = level;
    impl->parts[impl->part_no].data = part;  
    impl->parts[impl->part_no].data_size = size;  
    impl->part_no++;
}

void sspm_set_error(struct sspm_header* header, enum sspm_error error,
		    char* message)
{
    header->error = error;

    if(header->error_text!=0){
	free(header->error_text);
    }

    header->def = 0;

    if(message != 0){
	header->error_text = sspm_strdup(message);  
    } else {
	header->error_text = 0;
    }

}

void* sspm_make_part(struct mime_impl *impl,
		     struct sspm_header *header, 
		     struct sspm_header *parent_header,
		     void **end_part,
		     size_t *size)
{

    /* For a single part type, read to the boundary, if there is a
   boundary. Otherwise, read until the end of input.  This routine
   assumes that the caller has read the header and has left the input
   at the first blank line */

    char *line;
    void *part;
    int end = 0;

    struct sspm_action_map action = get_action(
	impl,
	header->major,
	header->minor);

    *size = 0;
    part =action.new_part();

    impl->state = IN_BODY;

    while(end == 0 && (line = sspm_get_next_line(impl)) != 0){
	
	if(sspm_is_mime_boundary(line)){
	    
	    /* If there is a boundary, then this must be a multipart
               part, so there must be a parent_header. */
	    if(parent_header == 0){
		char* boundary;
		end = 1;
		*end_part = 0;

		sspm_set_error(header,SSPM_UNEXPECTED_BOUNDARY_ERROR,line);

		/* Read until the paired terminating boundary */
		if((boundary = (char*)malloc(strlen(line)+5)) == 0){
		    fprintf(stderr,"Out of memory");
		    abort();
		}
		strcpy(boundary,line);
		strcat(boundary,"--");
		while((line = sspm_get_next_line(impl)) != 0){
		    /*printf("Error: %s\n",line);*/
		    if(strcmp(boundary,line)==0){
			break;
		    }
		}
		free(boundary);

		break;
	    }
	    
	    if(strncmp((line+2),parent_header->boundary,
		       sizeof(parent_header->boundary)) == 0){
		*end_part = action.end_part(part);

		if(sspm_is_mime_boundary(line)){
		    impl->state = END_OF_PART;
		} else if ( sspm_is_mime_terminating_boundary(line)){
		    impl->state = TERMINAL_END_OF_PART;
		}
		end = 1;
	    } else {
		/* Error, this is not the correct terminating boundary*/

		/* read and discard until we get the right boundary.  */
		    char* boundary;
		    char msg[256];

		    snprintf(msg,256,
			     "Expected: %s--. Got: %s",
			     parent_header->boundary,line);

		    sspm_set_error(parent_header,
		      SSPM_WRONG_BOUNDARY_ERROR,msg);

		    /* Read until the paired terminating boundary */
		    if((boundary = (char*)malloc(strlen(line)+5)) == 0){
			fprintf(stderr,"Out of memory");
			abort();
		    }		 
		    strcpy(boundary,line);
		    strcat(boundary,"--");
		    while((line = sspm_get_next_line(impl)) != 0){
			if(strcmp(boundary,line)==0){
			    break;
			}
		    }
		    free(boundary);

	    }	
	} else {
	    char* data=0;
	    char* rtrn=0;
	    *size = strlen(line);

	    data = (char*)malloc(*size+2);
	    assert(data != 0);
	    if (header->encoding == SSPM_BASE64_ENCODING){
		rtrn = decode_base64(data,line,size); 
	    } else if(header->encoding == SSPM_QUOTED_PRINTABLE_ENCODING){
		rtrn = decode_quoted_printable(data,line,size); 
	    } 

	    if(rtrn == 0){
		strcpy(data,line);
	    }

	    /* add a end-of-string after the data, just in case binary
               data from decode64 gets passed to a tring handling
               routine in add_line  */
	    data[*size+1]='\0';

	    action.add_line(part,header,data,*size);

	    free(data);
	}
    }

    if (end == 0){
	/* End the part if the input is exhausted */
	*end_part = action.end_part(part);
    }

    return end_part;
}


void* sspm_make_multipart_subpart(struct mime_impl *impl,
			    struct sspm_header *parent_header)
{
    struct sspm_header header;
    char *line;
    void* part;
    size_t size;

    if(parent_header->boundary == 0){
	/* Error. Multipart headers must have a boundary*/
	
	sspm_set_error(parent_header,SSPM_NO_BOUNDARY_ERROR,0);
	/* read all of the reamining lines */
	while((line = sspm_get_next_line(impl)) != 0){
	}  

	return 0;
    }


    /* Step 1: Read the opening boundary */

    if(get_line_type(impl->temp) != BOUNDARY){
	while((line=sspm_get_next_line(impl)) != 0 ){
	    if(sspm_is_mime_boundary(line)){

		assert(parent_header != 0);

		/* Check if it is the right boundary */
		if(!sspm_is_mime_terminating_boundary(line) &&
		   strncmp((line+2),parent_header->boundary, 
			   sizeof(parent_header->boundary)) 
		   == 0){
		    /* The +2 in strncmp skips over the leading "--" */
		    
		    break;
		} else {
		    /* Got the wrong boundary, so read and discard
                       until we get the right boundary.  */
		    char* boundary;
		    char msg[256];
		    
		    snprintf(msg,256,
			     "Expected: %s. Got: %s",
			     parent_header->boundary,line);

		    sspm_set_error(parent_header,
				   SSPM_WRONG_BOUNDARY_ERROR,msg);

		    /* Read until the paired terminating boundary */
		    if((boundary = (char*)malloc(strlen(line)+5)) == 0){
			fprintf(stderr,"Out of memory");
			abort();
		    }
		    strcpy(boundary,line);
		    strcat(boundary,"--");
		    while((line = sspm_get_next_line(impl)) != 0){
			if(strcmp(boundary,line)==0){
			    break;
			}
		    }
		    free(boundary);
		    
		    return 0;
		}
	    }
	}
    }

    /* Step 2: Get the part header */
    sspm_read_header(impl,&header);

    /* If the header is still listed as default, there was probably an
       error */
    if(header.def == 1 && header.error != SSPM_NO_ERROR){
	sspm_set_error(&header,SSPM_NO_HEADER_ERROR,0);
	return 0;
    }

    if(header.error!= SSPM_NO_ERROR){
	sspm_store_part(impl,header,impl->level,0,0);
	return 0;
    }	

    /* Step 3: read the body */
    
    if(header.major == SSPM_MULTIPART_MAJOR_TYPE){
	struct sspm_header *child_header;
	child_header = &(impl->parts[impl->part_no].header);

	/* Store the multipart part */
	sspm_store_part(impl,header,impl->level,0,0);

	/* now get all of the sub-parts */
	part = sspm_make_multipart_part(impl,child_header);

	if(get_line_type(impl->temp) != TERMINATING_BOUNDARY){

	    sspm_set_error(child_header,SSPM_NO_BOUNDARY_ERROR,impl->temp);
	    return 0;
	}
	
	sspm_get_next_line(impl); /* Step past the terminating boundary */

    } else {
	sspm_make_part(impl, &header,parent_header,&part,&size);

	memset(&(impl->parts[impl->part_no]), 0, sizeof(struct sspm_part));

	sspm_store_part(impl,header,impl->level,part,size);

    }

    return part;
}

void* sspm_make_multipart_part(struct mime_impl *impl,struct sspm_header *header)
{
    void *part=0;

    /* Now descend a level into each of the children of this part */
    impl->level++;

    /* Now we are working on the CHILD */
    memset(&(impl->parts[impl->part_no]), 0, sizeof(struct sspm_part));

    do{
	part = sspm_make_multipart_subpart(impl,header);

	if (part==0){
	    /* Clean up the part in progress */
	    impl->parts[impl->part_no].header.major 
		= SSPM_NO_MAJOR_TYPE;
	    impl->parts[impl->part_no].header.minor 
		= SSPM_NO_MINOR_TYPE;

	}
	

    } while (get_line_type(impl->temp) != TERMINATING_BOUNDARY &&
	impl->state != END_OF_INPUT);

    impl->level--;

    return 0;
}


void sspm_read_header(struct mime_impl *impl,struct sspm_header *header)
{
#define BUF_SIZE 1024
#define MAX_HEADER_LINES 25

    char *buf;
    char header_lines[MAX_HEADER_LINES][BUF_SIZE]; /* HACK, hard limits */
    int current_line = -1;
    int end = 0;

    memset(header_lines,0,sizeof(header_lines));
    memset(header,0,sizeof(struct sspm_header));

    /* Set up default header */
    header->def = 1;
    header->major = SSPM_TEXT_MAJOR_TYPE;
    header->minor = SSPM_PLAIN_MINOR_TYPE;
    header->error = SSPM_NO_ERROR;
    header->error_text = 0;

    /* Read all of the lines into memory */
    while(end==0&& (buf=sspm_get_next_line(impl)) != 0){

	enum line_type line_type = get_line_type(buf);
	
	switch(line_type){
	    case BLANK: {
		end = 1;
		impl->state = END_OF_HEADER;
		break;
	    }

	    case MAIL_HEADER:
	    case MIME_HEADER: {	    
		impl->state = IN_HEADER;
		current_line++;
		
		assert(strlen(buf) < BUF_SIZE);
		
		strncpy(header_lines[current_line],buf,BUF_SIZE-1);
		header_lines[current_line][BUF_SIZE-1] = '\0';
		
		break;
	    }
	    
	    case HEADER_CONTINUATION: {
		char* last_line, *end;
		char *buf_start;

		if(current_line < 0){
		    /* This is not really a continuation line, since
                       we have not see any header line yet */
		    sspm_set_error(header,SSPM_MALFORMED_HEADER_ERROR,buf);
		    return;
		}

		last_line = header_lines[current_line];
		end = (char*) ( (size_t)strlen(last_line)+
				      (size_t)last_line);
		
		impl->state = IN_HEADER;

		
		/* skip over the spaces in buf start, and remove the new
		   line at the end of the lat line */
		if (last_line[strlen(last_line)-1] == '\n'){
		    last_line[strlen(last_line)-1] = '\0';
		}
		buf_start = buf;
		while(*buf_start == ' ' ||*buf_start == '\t' ){
		    buf_start++;
		}
		
		assert( strlen(buf_start) + strlen(last_line) < BUF_SIZE);
		
		strncat(last_line,buf_start,BUF_SIZE-strlen(last_line)-1);
		
		break;
	    }
	    
	    default: {
		sspm_set_error(header,SSPM_MALFORMED_HEADER_ERROR,buf);
		return;
	    }
	}
    }
	

    for(current_line = 0;
	current_line < MAX_HEADER_LINES && header_lines[current_line][0] != 0;
	current_line++){
	
	sspm_build_header(header,header_lines[current_line]);
    }


}

/* Root routine for parsing mime entries*/
int sspm_parse_mime(struct sspm_part *parts, 
		    size_t max_parts,
		    struct sspm_action_map *actions,
		    char* (*get_string)(char *s, size_t size, void* data),
		    void *get_string_data,
		    struct sspm_header *first_header
    )
{
    struct mime_impl impl;
    struct sspm_header header;
    void *part;
    int i;

    /* Initialize all of the data */
    memset(&impl,0,sizeof(struct mime_impl));
    memset(&header,0,sizeof(struct sspm_header));

    for(i = 0; i<(int)max_parts; i++){
	parts[i].header.major = SSPM_NO_MAJOR_TYPE;
	parts[i].header.minor = SSPM_NO_MINOR_TYPE;
    }
	
    impl.parts = parts;
    impl.max_parts = max_parts;
    impl.part_no = 0;
    impl.actions = actions;
    impl.get_string = get_string;
    impl.get_string_data = get_string_data;

    /* Read the header of the message. This will be the email header,
       unless first_header is specified. But ( HACK) that var is not
       currently being used */
    sspm_read_header(&impl,&header);

    if(header.major == SSPM_MULTIPART_MAJOR_TYPE){
	struct sspm_header *child_header;
	child_header = &(impl.parts[impl.part_no].header);
	
	sspm_store_part(&impl,header,impl.level,0,0);

	part = sspm_make_multipart_part(&impl,child_header);

    } else {
	void *part;
	size_t size;
	sspm_make_part(&impl, &header, 0,&part,&size);

	memset(&(impl.parts[impl.part_no]), 0, sizeof(struct sspm_part));
	
	sspm_store_part(&impl,header,impl.level,part,size);
    }

    return 0;
}

void sspm_free_parts(struct sspm_part *parts, size_t max_parts)
{
     int i;
    
    for(i = 0; i<(int)max_parts && parts[i].header.major != SSPM_NO_MAJOR_TYPE;
	i++){
	sspm_free_header(&(parts[i].header));
    }
}

void sspm_free_header(struct sspm_header *header)
{
    if(header->boundary!=0){
	free(header->boundary);
    }
    if(header->minor_text!=0){
	free(header->minor_text);
    }
    if(header->charset!=0){
	free(header->charset);
    }
    if(header->filename!=0){
	free(header->filename);
    }
    if(header->content_id!=0){
	free(header->content_id);
    }
    if(header->error_text!=0){
	free(header->error_text);
    }
}

/***********************************************************************
The remaining code is beased on code from the mimelite distribution,
which has the following notice:

| Authorship:
|    Copyright (c) 1994 Gisle Hannemyr.
|    Permission is granted to hack, make and distribute copies of this
|    program as long as this copyright notice is not removed.
|    Flames, bug reports, comments and improvements to:
|       snail: Gisle Hannemyr, Brageveien 3A, 0452 Oslo, Norway
|       email: Inet: gisle@oslonett.no

The code is heavily modified by Eric Busboom. 

***********************************************************************/

char *decode_quoted_printable(char *dest, 
				       char *src,
				       size_t *size)
{
    int cc;
    size_t i=0;

    while (*src != 0 && i < *size) {
	if (*src == '=') {

	    src++; 
	    if (!*src) {
		break;
	    }

	    /* remove soft line breaks*/
	    if ((*src == '\n') || (*src == '\r')){
		src++;
		if ((*src == '\n') || (*src == '\r')){
		    src++;
		}
		continue;
	    }

	    cc  = isdigit(*src) ? (*src - '0') : (*src - 55);
	    cc *= 0x10;
	    src++; 
	    if (!*src) {
		break;
	    }
	    cc += isdigit(*src) ? (*src - '0') : (*src - 55);

	    *dest = cc;

	} else {
	    *dest = *src;
	}
	
	dest++;
	src++;
	i++;
    }
    
    *dest = '\0';
    
    *size = i;
    return(dest);
}

char *decode_base64(char *dest, 
			     char *src,
			     size_t *size)
{
    int cc = 0;
    char buf[4] = {0,0,0,0};  
    int p = 0;
    int valid_data = 0;
    size_t size_out=0;
    
    while (*src && p<(int)*size && (cc!=  -1)) {
	
	/* convert a character into the Base64 alphabet */
	cc = *src++;
	
	if	((cc >= 'A') && (cc <= 'Z')) cc = cc - 'A';
	else if ((cc >= 'a') && (cc <= 'z')) cc = cc - 'a' + 26;
	else if ((cc >= '0') && (cc <= '9')) cc = cc - '0' + 52;
	else if  (cc == '/')		     cc = 63;
	else if  (cc == '+')		     cc = 62;
	else                                 cc = -1;
	
	assert(cc<64);

	/* If we've reached the end, fill the remaining slots in
	   the bucket and do a final conversion */
	if(cc== -1){
	    if(valid_data == 0){
		return 0;
	    }

	    while(p%4!=3){
		p++;
		buf[p%4] = 0;
	    }
	} else {
	    buf[p%4] = cc;
	    size_out++;
	    valid_data = 1;
	}

	
	/* When we have 4 base64 letters, convert them into three
	   bytes */
	if (p%4 == 3) {
	    *dest++ =(buf[0]<< 2)|((buf[1] & 0x30) >> 4);
	    *dest++ =((buf[1] & 0x0F) << 4)|((buf[2] & 0x3C) >> 2);
	    *dest++ =((buf[2] & 0x03) << 6)|(buf[3] & 0x3F);

	    memset(buf,0,4);
	}

	p++;

    }
    /* Calculate the size of the converted data*/
   *size = ((int)(size_out/4))*3;
    if(size_out%4 == 2) *size+=1;
    if(size_out%4 == 3) *size+=2;

    return(dest);
}


/***********************************************************************
								       
 Routines to output MIME

**********************************************************************/


struct sspm_buffer {
	char* buffer;
	char* pos;
	size_t buf_size;
	int line_pos;
};

void sspm_append_string(struct sspm_buffer* buf, char* string);
void sspm_write_part(struct sspm_buffer *buf,struct sspm_part *part, int *part_num);

void sspm_append_hex(struct sspm_buffer* buf, char ch)
{
    char tmp[4];

    snprintf(tmp,sizeof(tmp),"=%02X",ch);

    sspm_append_string(buf,tmp);
}

/* a copy of icalmemory_append_char */
void sspm_append_char(struct sspm_buffer* buf, char ch)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length;

    data_length = (size_t)buf->pos - (size_t)buf->buffer;

    final_length = data_length + 2; 

    if ( final_length > (size_t) buf->buf_size ) {
	
	buf->buf_size  = (buf->buf_size) * 2  + final_length +1;

	new_buf = realloc(buf->buffer,buf->buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	buf->pos = new_pos;
	buf->buffer = new_buf;
    }

    *(buf->pos) = ch;
    buf->pos += 1;
    *(buf->pos) = 0;
}
/* A copy of icalmemory_append_string */
void sspm_append_string(struct sspm_buffer* buf, char* string)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length, string_length;

    string_length = strlen(string);
    data_length = (size_t)buf->pos - (size_t)buf->buffer;    
    final_length = data_length + string_length; 

    if ( final_length >= (size_t) buf->buf_size) {

	
	buf->buf_size  = (buf->buf_size) * 2  + final_length;

	new_buf = realloc(buf->buffer,buf->buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	buf->pos = new_pos;
	buf->buffer = new_buf;
    }
    
    strcpy(buf->pos, string);

    buf->pos += string_length;
}



static int sspm_is_printable(char c)
{
    return (c >= 33) && (c <= 126) && (c != '=');

} 
                     

void sspm_encode_quoted_printable(struct sspm_buffer *buf, char* data)
{
    char *p;
    int lpos = 0;

    for(p = data; *p != 0; p++){

	if(sspm_is_printable(*p)){
	    /* plain characters can represent themselves */
	    /* RFC2045 Rule #2 */
	       sspm_append_char(buf,*p);
	       lpos++;
	} else if ( *p == '\t' || *p == ' ' ) {

	    /* For tabs and spaces, only encode if they appear at the
               end of the line */
	    /* RFC2045 Rule #3 */

	   char n = *(p+1);

	   if( n == '\n' || n == '\r'){
	       sspm_append_hex(buf,*p);
	       lpos += 3;
	   } else {
	       sspm_append_char(buf,*p);
	       lpos++;
	   }

	} else if( *p == '\n' || *p == '\r'){
	    sspm_append_char(buf,*p);

	    lpos=0;

	} else {
	    /* All others need to be encoded */
	    sspm_append_hex(buf,*p);
	    lpos+=3;
	}


	/* Add line breaks */
	if (lpos > 72){
	    lpos = 0;
	    sspm_append_string(buf,"=\n");
	}
    }
}

static char BaseTable[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};
    
void sspm_write_base64(struct sspm_buffer *buf, char* inbuf,int size )
{
    
    char outbuf[4];
    int i;

    outbuf[0] = outbuf[1] = outbuf[2] = outbuf[3] = 65;

    switch(size){
	
	case 4:
	    outbuf[3] =   inbuf[2] & 0x3F;

	case 3:
	    outbuf[2] = ((inbuf[1] & 0x0F) << 2) | ((inbuf[2] & 0xC0) >> 6);

	case 2:	
	    outbuf[0] =  (inbuf[0] & 0xFC) >> 2;
	    outbuf[1] = ((inbuf[0] & 0x03) << 4) | ((inbuf[1] & 0xF0) >> 4);
	    break;

	default:
	    assert(0);
    }

    for(i = 0; i < 4; i++){

	if(outbuf[i] == 65){
	    sspm_append_char(buf,'=');
	} else {
	    sspm_append_char(buf,BaseTable[(int)outbuf[i]]);
	}
    }
}
             
void sspm_encode_base64(struct sspm_buffer *buf, char* data, size_t size)
{

    char *p;
    char inbuf[3];
    int i = 0;
    int first = 1;
    int lpos = 0;

    inbuf[0] = inbuf[1] = inbuf[2]  = 0;

    for (p = data; *p !=0; p++){
                         
	if (i%3 == 0 && first == 0){

	    sspm_write_base64(buf, inbuf, 4);
	    lpos+=4;

	    inbuf[0] = inbuf[1] = inbuf[2] = 0;
	}

	assert(lpos%4 == 0);

	if (lpos == 72){
	    sspm_append_string(buf,"\n");
	    lpos = 0;
	}

	inbuf[i%3] = *p;

	i++;
	first = 0;

    }

    
    /* If the inbuf was not exactly filled on the last byte, we need
       to spit out the odd bytes that did get in -- either one or
       two. This will result in an output of two bytes and '==' or
       three bytes and '=', respectively */
    
    if (i%3 == 1 && first == 0){
	    sspm_write_base64(buf, inbuf, 2);
    } else if (i%3 == 2 && first == 0){
	    sspm_write_base64(buf, inbuf, 3);
    }

}

void sspm_write_header(struct sspm_buffer *buf,struct sspm_header *header)
{
    
    int i;
    char temp[TMP_BUF_SIZE];			       
    char* major; 
    char* minor; 
    
    /* Content-type */

    major = sspm_major_type_string(header->major);
    minor = sspm_minor_type_string(header->minor);

    if(header->minor == SSPM_UNKNOWN_MINOR_TYPE ){
	assert(header->minor_text !=0);
	minor = header->minor_text;
    }
    
    snprintf(temp,sizeof(temp),"Content-Type: %s/%s",major,minor);

    sspm_append_string(buf,temp);

    if(header->boundary != 0){
	snprintf(temp,sizeof(temp),";boundary=\"%s\"",header->boundary);
	sspm_append_string(buf,temp);
    }
    
    /* Append any content type parameters */    
    if(header->content_type_params != 0){
	for(i=0; *(header->content_type_params[i])!= 0;i++){
	    snprintf(temp,sizeof(temp),header->content_type_params[i]);
	    sspm_append_char(buf,';');
	    sspm_append_string(buf,temp);
	}
    }
    
    sspm_append_char(buf,'\n');

    /*Content-Transfer-Encoding */

    if(header->encoding != SSPM_UNKNOWN_ENCODING &&
	header->encoding != SSPM_NO_ENCODING){
	snprintf(temp,sizeof(temp),"Content-Transfer-Encoding: %s\n",
		sspm_encoding_string(header->encoding));
    }

    sspm_append_char(buf,'\n');

}

void sspm_write_multipart_part(struct sspm_buffer *buf,
			       struct sspm_part *parts,
			       int* part_num)
{

    int parent_level, level;
    struct sspm_header *header = &(parts[*part_num].header);
    /* Write the header for the multipart part */
    sspm_write_header(buf,header);

    parent_level = parts[*part_num].level;

    (*part_num)++;

    level = parts[*part_num].level;

    while(parts[*part_num].header.major != SSPM_NO_MAJOR_TYPE &&
	  level == parent_level+1){

	assert(header->boundary);
	sspm_append_string(buf,header->boundary);
	sspm_append_char(buf,'\n');
	
	if (parts[*part_num].header.major == SSPM_MULTIPART_MAJOR_TYPE){
	    sspm_write_multipart_part(buf,parts,part_num);
	} else {
	    sspm_write_part(buf, &(parts[*part_num]), part_num);
	}	

	(*part_num)++;
	level =  parts[*part_num].level;
    }
   
    sspm_append_string(buf,"\n\n--");
    sspm_append_string(buf,header->boundary);
    sspm_append_string(buf,"\n");

    (*part_num)--; /* undo last, spurious, increment */
}

void sspm_write_part(struct sspm_buffer *buf,struct sspm_part *part,int *part_num)
{

    /* Write header */
    sspm_write_header(buf,&(part->header));

    /* Write part data */

    if(part->data == 0){
	return;
    }

    if(part->header.encoding == SSPM_BASE64_ENCODING) {
	assert(part->data_size != 0);
	sspm_encode_base64(buf,part->data,part->data_size);
    } else if(part->header.encoding == SSPM_QUOTED_PRINTABLE_ENCODING) {
	sspm_encode_quoted_printable(buf,part->data);
    } else {
	sspm_append_string(buf,part->data);
    }

    sspm_append_string(buf,"\n\n");
}

int sspm_write_mime(struct sspm_part *parts,size_t num_parts,
		    char **output_string, char* header)
{
    struct sspm_buffer buf;
    int part_num =0;

    buf.buffer = malloc(4096);
    buf.pos = buf.buffer;
    buf.buf_size = 10;
    buf.line_pos = 0;

    /* write caller's header */
    if(header != 0){
	sspm_append_string(&buf,header);
    }

    if(buf.buffer[strlen(buf.buffer)-1] != '\n'){
	sspm_append_char(&buf,'\n');
    }

    /* write mime-version header */
    sspm_append_string(&buf,"Mime-Version: 1.0\n");

    /* End of header */

    /* Write body parts */
    while(parts[part_num].header.major != SSPM_NO_MAJOR_TYPE){
	if (parts[part_num].header.major == SSPM_MULTIPART_MAJOR_TYPE){
	    sspm_write_multipart_part(&buf,parts,&part_num);
	} else {
	    sspm_write_part(&buf, &(parts[part_num]), &part_num);
	}	

	part_num++;
    }


    *output_string = buf.buffer;

    return 0;
}

