/* -*- Mode: C; tab-width: 4; c-basic-offset: 8; -*-
  ======================================================================
  FILE: icalparser.c
  CREATOR: eric 04 August 1999
  
  $Id: icalparser.c,v 1.48 2005/01/24 12:51:37 acampi Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "pvl.h"
#include "icalerror.h"
#include "icalvalue.h"
#include "icalderivedparameter.h"
#include "icalparameter.h"
#include "icalproperty.h"
#include "icalcomponent.h"

#include <string.h> /* For strncpy & size_t */
#include <stdio.h> /* For FILE and fgets and snprintf */
#include <stdlib.h> /* for free */

#include "icalmemory.h"
#include "icalparser.h"

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

char* icalparser_get_next_char(char c, char *str, int qm);
char* icalparser_get_next_parameter(char* line,char** end);
char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind);
char* icalparser_get_prop_name(char* line, char** end);
char* icalparser_get_param_name_and_value(char* line, char **value);

#define TMP_BUF_SIZE 80

struct icalparser_impl 
{
    int buffer_full; /* flag indicates that temp is smaller that 
                        data being read into it*/
    int continuation_line; /* last line read was a continuation line */
    size_t tmp_buf_size; 
    char temp[TMP_BUF_SIZE];
    icalcomponent *root_component;
    int version; 
    int level;
    int lineno;
    icalparser_state state;
    pvl_list components;
    
    void *line_gen_data;

};


icalparser* icalparser_new(void)
{
    struct icalparser_impl* impl = 0;
    if ( ( impl = (struct icalparser_impl*)
	   malloc(sizeof(struct icalparser_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    impl->buffer_full = 0;
    impl->continuation_line = 0;
    impl->tmp_buf_size = sizeof(impl->temp);
    memset(impl->temp, 0, sizeof(impl->temp));
    impl->root_component = 0;
    impl->version = 0;
    impl->level = 0;
    impl->lineno = 0;
    impl->state = ICALPARSER_SUCCESS;
    impl->components  = pvl_newlist();  
    impl->line_gen_data = 0;

    return (icalparser*)impl;
}


void icalparser_free(icalparser* parser)
{
    icalcomponent *c;

    if (parser->root_component != 0){
	icalcomponent_free(parser->root_component);
    }

    while( (c=pvl_pop(parser->components)) != 0){
	icalcomponent_free(c);
    }
    
    pvl_free(parser->components);
    
    free(parser);
}

void icalparser_set_gen_data(icalparser* parser, void* data)
{
		parser->line_gen_data  = data;
}


icalvalue* icalvalue_new_From_string_with_error(icalvalue_kind kind, 
                                                char* str, 
                                                icalproperty **error);



char* icalparser_get_next_char(char c, char *str, int qm)
{
    int quote_mode = 0;
    char* p;

    for(p=str; *p!=0; p++){
	    if (qm == 1) {
				if ( quote_mode == 0 && *p=='"' && *(p-1) != '\\' ){
						quote_mode =1;
						continue;
				}

				if ( quote_mode == 1 && *p=='"' && *(p-1) != '\\' ){
						quote_mode =0;
						continue;
				}
	    }
		
		if (quote_mode == 0 &&  *p== c  && *(p-1) != '\\' ){
				return p;
		} 

    }

    return 0;
}


/** make a new tmp buffer out of a substring */
static char* make_segment(char* start, char* end)
{
    char *buf;
    size_t size = (size_t)end - (size_t)start;
    
    buf = icalmemory_new_buffer(size+1);
    

    strncpy(buf,start,size);
    *(buf+size) = 0;

    return buf;
}


char* icalparser_get_prop_name(char* line, char** end)
{
    char* p;
    char* v;
    char *str;

    p = icalparser_get_next_char(';',line,1); 
    v = icalparser_get_next_char(':',line,1); 
    if (p== 0 && v == 0) {
	return 0;
    }

    /* There is no ';' or, it is after the ';' that marks the beginning of
       the value */
    if (v!=0 && ( p == 0 || p > v)){
	str = make_segment(line,v);
	*end = v+1;
    } else {
	str = make_segment(line,p);
	*end = p+1;
    }

    return str;
}


char* icalparser_get_param_name_and_value(char* line, char **value)
{
    char* next; 
    char *str;
    char **end = value;

    *value = NULL;
    next = icalparser_get_next_char('=',line,1);

    if (next == 0) {
	return 0;
    }

    str = make_segment(line,next);
    *end = next+1;
    if (**end == '"') {
        *end = *end+1;
	    next = icalparser_get_next_char('"',*end,0);
	    if (next == 0) {
		    return 0;
	    }

	    *end = make_segment(*end,next);
    }
    else
	*end = make_segment(*end, *end + strlen(*end));

    return str;
}


char* icalparser_get_next_paramvalue(char* line, char **end)
{
    char* next; 
    char *str;

    next = icalparser_get_next_char(',',line,1);

    if (next == 0){
	next = (char*)(size_t)line+(size_t)strlen(line);\
    }

    if (next == line){
	return 0;
    } else {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    }
}


/**
   A property may have multiple values, if the values are seperated by
   commas in the content line. This routine will look for the next
   comma after line and will set the next place to start searching in
   end. */

char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind)
{
    
    char* next;
    char *p;
    char *str;
    size_t length = strlen(line);

    p = line;
    while(1){

	next = icalparser_get_next_char(',',p,1);

	/* Unforunately, RFC2445 says that for the RECUR value, COMMA
	   can both seperate digits in a list, and it can seperate
	   multiple recurrence specifications. This is not a friendly
	   part of the spec. This weirdness tries to
	   distinguish the two uses. it is probably a HACK*/
      
	if( kind == ICAL_RECUR_VALUE ) {
	    if ( next != 0 &&
		 (*end+length) > next+5 &&
		 strncmp(next,"FREQ",4) == 0
		) {
		/* The COMMA was followed by 'FREQ', is it a real seperator*/
		/* Fall through */
	    } else if (next != 0){
		/* Not real, get the next COMMA */
		p = next+1;
		next = 0;
		continue;
	    } 
	}
	/* ignore all , for query value. select dtstart, dtend etc ... */
	else if( kind == ICAL_QUERY_VALUE) {
	    if ( next != 0) {
		p = next+1;
		continue;
	    }
	    else
		break;
	}

	/* If the comma is preceeded by a '\', then it is a literal and
	   not a value seperator*/  
      
	if ( (next!=0 && *(next-1) == '\\') ||
	     (next!=0 && *(next-3) == '\\')
	    ) 
	    /*second clause for '/' is on prev line. HACK may be out of bounds */
	{
	    p = next+1;
	} else {
	    break;
	}

    }

    if (next == 0){
	next = (char*)(size_t)line+length;
	*end = next;
    } else {
	*end = next+1;
    }

    if (next == line){
	return 0;
    } 
	

    str = make_segment(line,next);
    return str;
   
}

char* icalparser_get_next_parameter(char* line,char** end)
{
    char *next;
    char *v;
    char *str;

    v = icalparser_get_next_char(':',line,1); 
    next = icalparser_get_next_char(';', line,1);
    
    /* There is no ';' or, it is after the ':' that marks the beginning of
       the value */

    if (next == 0 || next > v) {
	next = icalparser_get_next_char(':', line,1);
    }

    if (next != 0) {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    } else {
	*end = line;
	return 0;
    }
}


/**
 * Get a single property line, from the property name through the
 * final new line, and include any continuation lines
 */
char* icalparser_get_line(icalparser *parser,
                          char* (*line_gen_func)(char *s, size_t size, void *d))
{
    char *line;
    char *line_p;
    size_t buf_size = parser->tmp_buf_size;
    
    line_p = line = icalmemory_new_buffer(buf_size);
    line[0] = '\0';
   
    /* Read lines by calling line_gen_func and putting the data into
       parser->temp. If the line is a continuation line ( begins with a
       space after a newline ) then append the data onto line and read
       again. Otherwise, exit the loop. */

    while(1) {

        /* The first part of the loop deals with the temp buffer,
           which was read on he last pass through the loop. The
           routine is split like this because it has to read lone line
           ahead to determine if a line is a continuation line. */


	/* The tmp buffer is not clear, so transfer the data in it to the
	   output. This may be left over from a previous call */
	if (parser->temp[0] != '\0' ) {

	    /* If the last position in the temp buffer is occupied,
               mark the buffer as full. The means we will do another
               read later, because the line is not finished */
	    if (parser->temp[parser->tmp_buf_size-1] == 0 &&
		parser->temp[parser->tmp_buf_size-2] != '\n'&& 
                parser->temp[parser->tmp_buf_size-2] != 0 ){
		parser->buffer_full = 1;
	    } else {
		parser->buffer_full = 0;
	    }

	    /* Copy the temp to the output and clear the temp buffer. */
            if(parser->continuation_line==1){
                /* back up the pointer to erase the continuation characters */
                parser->continuation_line = 0;
                line_p--;
                
                if ( *(line_p-1) == '\r'){
                    line_p--;
                }
                
                /* copy one space up to eliminate the leading space*/
                icalmemory_append_string(&line,&line_p,&buf_size,
                                         parser->temp+1);

            } else {
                icalmemory_append_string(&line,&line_p,&buf_size,parser->temp);
            }

            parser->temp[0] = '\0' ;
	}
	
	parser->temp[parser->tmp_buf_size-1] = 1; /* Mark end of buffer */

        /****** Here is where the routine gets string data ******************/
	if ((*line_gen_func)(parser->temp,parser->tmp_buf_size,parser->line_gen_data)
	    ==0){/* Get more data */

	    /* If the first position is clear, it means we didn't get
               any more data from the last call to line_ge_func*/
	    if (parser->temp[0] == '\0'){

		if(line[0] != '\0'){
		    /* There is data in the output, so fall trhough and process it*/
		    break;
		} else {
		    /* No data in output; return and signal that there
                       is no more input*/
		    free(line);
		    return 0;
		}
	    }
	}
	
 
	/* If the output line ends in a '\n' and the temp buffer
	   begins with a ' ', then the buffer holds a continuation
	   line, so keep reading.  */

	if ( line_p > line+1 && *(line_p-1) == '\n'
	  && (parser->temp[0] == ' ' || parser->temp[0] == '\t') ) {

            parser->continuation_line = 1;

	} else if ( parser->buffer_full == 1 ) {
	    
	    /* The buffer was filled on the last read, so read again */

	} else {

	    /* Looks like the end of this content line, so break */
	    break;
	}

	
    }

    /* Erase the final newline and/or carriage return*/
    if ( line_p > line+1 && *(line_p-1) == '\n') {	
	*(line_p-1) = '\0';
	if ( *(line_p-2) == '\r'){
	    *(line_p-2) = '\0';
	}

    } else {
	*(line_p) = '\0';
    }

    return line;

}

static void insert_error(icalcomponent* comp, char* text, 
		  char* message, icalparameter_xlicerrortype type)
{
    char temp[1024];
    
    if (text == 0){
	snprintf(temp,1024,"%s:",message);
    } else {
	snprintf(temp,1024,"%s: %s",message,text);
    }	
    
    icalcomponent_add_property
	(comp,
	 icalproperty_vanew_xlicerror(
	     temp,
	     icalparameter_new_xlicerrortype(type),
	     0));   
}


static int line_is_blank(char* line){
    int i=0;

    for(i=0; *(line+i)!=0; i++){
	char c = *(line+i);

	if(c != ' ' && c != '\n' && c != '\t'){
	    return 0;
	}
    }
    
    return 1;
}

icalcomponent* icalparser_parse(icalparser *parser,
				char* (*line_gen_func)(char *s, size_t size, 
						       void* d))
{

    char* line; 
    icalcomponent *c=0; 
    icalcomponent *root=0;
    icalerrorstate es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
	int cont;

    icalerror_check_arg_rz((parser !=0),"parser");

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);

    do{
	    line = icalparser_get_line(parser, line_gen_func);

	if ((c = icalparser_add_line(parser,line)) != 0){

	    if(icalcomponent_get_parent(c) !=0){
		/* This is bad news... assert? */
	    }	    
	    
	    assert(parser->root_component == 0);
	    assert(pvl_count(parser->components) ==0);

	    if (root == 0){
		/* Just one component */
		root = c;
	    } else if(icalcomponent_isa(root) != ICAL_XROOT_COMPONENT) {
		/*Got a second component, so move the two components under
		  an XROOT container */
		icalcomponent *tempc = icalcomponent_new(ICAL_XROOT_COMPONENT);
		icalcomponent_add_component(tempc, root);
		icalcomponent_add_component(tempc, c);
		root = tempc;
	    } else if(icalcomponent_isa(root) == ICAL_XROOT_COMPONENT) {
		/* Already have an XROOT container, so add the component
		   to it*/
		icalcomponent_add_component(root, c);
		
	    } else {
		/* Badness */
		assert(0);
	    }

	    c = 0;

        }
	cont = 0;
	if(line != 0){
	    icalmemory_free_buffer(line);
		cont = 1;
	}
    } while ( cont );

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);

    return root;

}


icalcomponent* icalparser_add_line(icalparser* parser,
                                       char* line)
{ 
    char *str;
    char *end;
    int vcount = 0;
    icalproperty *prop;
    icalproperty_kind prop_kind;
    icalvalue *value;
    icalvalue_kind value_kind = ICAL_NO_VALUE;


    icalerror_check_arg_rz((parser != 0),"parser");


    if (line == 0)
    {
	parser->state = ICALPARSER_ERROR;
	return 0;
    }

    if(line_is_blank(line) == 1){
	return 0;
    }

    /* Begin by getting the property name at the start of the line. The
       property name may end up being "BEGIN" or "END" in which case it
       is not really a property, but the marker for the start or end of
       a component */

    end = 0;
    str = icalparser_get_prop_name(line, &end);

    if (str == 0 || strlen(str) == 0 ){
	/* Could not get a property name */
	icalcomponent *tail = pvl_data(pvl_tail(parser->components));

	if (tail){
	    insert_error(tail,line,
			 "Got a data line, but could not find a property name or component begin tag",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	}
	tail = 0;
	parser->state = ICALPARSER_ERROR;
	return 0; 
    }

    /**********************************************************************
     * Handle begin and end of components
     **********************************************************************/								       
    /* If the property name is BEGIN or END, we are actually
       starting or ending a new component */


    if(strcmp(str,"BEGIN") == 0){
	icalcomponent *c;
        icalcomponent_kind comp_kind;

	icalmemory_free_buffer(str);
	parser->level++;
	str = icalparser_get_next_value(end,&end, value_kind);
	    

        comp_kind = icalenum_string_to_component_kind(str);


        if (comp_kind == ICAL_NO_COMPONENT){


	    c = icalcomponent_new(ICAL_XLICINVALID_COMPONENT);
	    insert_error(c,str,"Parse error in component name",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
        }

	c  =  icalcomponent_new(comp_kind);

	if (c == 0){
	    c = icalcomponent_new(ICAL_XLICINVALID_COMPONENT);
	    insert_error(c,str,"Parse error in component name",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	}
	    
	pvl_push(parser->components,c);

	parser->state = ICALPARSER_BEGIN_COMP;
	if (str)
	    icalmemory_free_buffer(str);
	return 0;

    } else if (strcmp(str,"END") == 0 ) {
	icalcomponent* tail;

	icalmemory_free_buffer(str);
	parser->level--;
	str = icalparser_get_next_value(end,&end, value_kind);
	if (str)
	    icalmemory_free_buffer(str);

	/* Pop last component off of list and add it to the second-to-last*/
	parser->root_component = pvl_pop(parser->components);

	tail = pvl_data(pvl_tail(parser->components));

	if(tail != 0){
	    icalcomponent_add_component(tail,parser->root_component);
	} 

	tail = 0;

	/* Return the component if we are back to the 0th level */
	if (parser->level == 0){
	    icalcomponent *rtrn; 

	    if(pvl_count(parser->components) != 0){
	    /* There are still components on the stack -- this means
               that one of them did not have a proper "END" */
		pvl_push(parser->components,parser->root_component);
		icalparser_clean(parser); /* may reset parser->root_component*/
	    }

	    assert(pvl_count(parser->components) == 0);

	    parser->state = ICALPARSER_SUCCESS;
	    rtrn = parser->root_component;
	    parser->root_component = 0;
	    return rtrn;

	} else {
	    parser->state = ICALPARSER_END_COMP;
	    return 0;
	}
    }


    /* There is no point in continuing if we have not seen a
       component yet */

    if(pvl_data(pvl_tail(parser->components)) == 0){
	parser->state = ICALPARSER_ERROR;
	icalmemory_free_buffer(str);
	return 0;
    }


    /**********************************************************************
     * Handle property names
     **********************************************************************/
								       
    /* At this point, the property name really is a property name,
       (Not a component name) so make a new property and add it to
       the component */

    
    prop_kind = icalproperty_string_to_kind(str);

    prop = icalproperty_new(prop_kind);

    if (prop != 0){
	icalcomponent *tail = pvl_data(pvl_tail(parser->components));

        if(prop_kind==ICAL_X_PROPERTY){
            icalproperty_set_x_name(prop,str);
        }
	icalmemory_free_buffer(str);

	icalcomponent_add_property(tail, prop);

	/* Set the value kind for the default for this type of
	   property. This may be re-set by a VALUE parameter */
	value_kind = icalproperty_kind_to_value_kind(icalproperty_isa(prop));

    } else {
	icalcomponent* tail = pvl_data(pvl_tail(parser->components));

	insert_error(tail,str,"Parse error in property name",
		     ICAL_XLICERRORTYPE_PROPERTYPARSEERROR);
	    
	tail = 0;
	parser->state = ICALPARSER_ERROR;
	icalmemory_free_buffer(str);
	return 0;
    }

    /**********************************************************************
     * Handle parameter values
     **********************************************************************/								       

    /* Now, add any parameters to the last property */

    while(1) {

	if (*(end-1) == ':'){
	    /* if the last seperator was a ":" and the value is a
	       URL, icalparser_get_next_parameter will find the
	       ':' in the URL, so better break now. */
	    break;
	}

	str = icalparser_get_next_parameter(end,&end);

	if (str != 0){
	    char* name;
	    char* pvalue;
        
	    icalparameter *param = 0;
	    icalparameter_kind kind;
	    icalcomponent *tail = pvl_data(pvl_tail(parser->components));

	    name = icalparser_get_param_name_and_value(str,&pvalue);

	    if (name == 0){
		/* 'tail' defined above */
		insert_error(tail, str, "Cant parse parameter name",
			     ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR);
		tail = 0;
		icalmemory_free_buffer(str);
		if (pvalue)
		    icalmemory_free_buffer(pvalue);
		break;
	    }

	    kind = icalparameter_string_to_kind(name);

	    if(kind == ICAL_X_PARAMETER){
		param = icalparameter_new(ICAL_X_PARAMETER);
		
		if(param != 0){
		    icalparameter_set_xname(param,name);
		    icalparameter_set_xvalue(param,pvalue);
		}


	    } else if (kind != ICAL_NO_PARAMETER){
		param = icalparameter_new_from_value_string(kind,pvalue);
	    } else {
		/* Error. Failed to parse the parameter*/
		/* 'tail' defined above */
		insert_error(tail, str, "Cant parse parameter name",
			     ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR);
		tail = 0;
		parser->state = ICALPARSER_ERROR;
		icalmemory_free_buffer(str);
		icalmemory_free_buffer(name);
		if (pvalue)
		    icalmemory_free_buffer(pvalue);
		return 0;
	    }
	    icalmemory_free_buffer(name);
	    if (pvalue)
		icalmemory_free_buffer(pvalue);


	    if (param == 0){
		/* 'tail' defined above */
		insert_error(tail,str,"Cant parse parameter value",
			     ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR);
		    
		tail = 0;
		parser->state = ICALPARSER_ERROR;
		icalmemory_free_buffer(str);
		continue;
	    }

	    /* If it is a VALUE parameter, set the kind of value*/
	    if (icalparameter_isa(param)==ICAL_VALUE_PARAMETER){

		value_kind = (icalvalue_kind)
                    icalparameter_value_to_value_kind(
                                icalparameter_get_value(param)
                                );

		if (value_kind == ICAL_NO_VALUE){

		    /* Ooops, could not parse the value of the
		       parameter ( it was not one of the defined
		       values ), so reset the value_kind */
			
		    insert_error(
			tail, str, 
			"Got a VALUE parameter with an unknown type",
			ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR);
		    icalparameter_free(param);
			
		    value_kind = 
			icalproperty_kind_to_value_kind(
			    icalproperty_isa(prop));
			
		    icalparameter_free(param);
		    tail = 0;
		    parser->state = ICALPARSER_ERROR;
		    icalmemory_free_buffer(str);
		    return 0;
		} 
	    }

	    /* Everything is OK, so add the parameter */
	    icalproperty_add_parameter(prop,param);
	    tail = 0;
	    icalmemory_free_buffer(str);

	} else { /* if ( str != 0)  */
	    /* If we did not get a param string, go on to looking
	       for a value */
	    break;
	} /* if ( str != 0)  */

    } /* while(1) */	    
	
    /**********************************************************************
     * Handle values
     **********************************************************************/								       

    /* Look for values. If there are ',' characters in the values,
       then there are multiple values, so clone the current
       parameter and add one part of the value to each clone */

    vcount=0;
    while(1) {
	str = icalparser_get_next_value(end,&end, value_kind);

	if (str != 0){
		
	    if (vcount > 0){
		/* Actually, only clone after the second value */
		icalproperty* clone = icalproperty_new_clone(prop);
		icalcomponent* tail = pvl_data(pvl_tail(parser->components));
		    
		icalcomponent_add_property(tail, clone);
		prop = clone;		    
		tail = 0;
	    }
		
	    value = icalvalue_new_from_string(value_kind, str);
		
	    /* Don't add properties without value */
	    if (value == 0){
		char temp[200]; /* HACK */

		icalproperty_kind prop_kind = icalproperty_isa(prop);
		icalcomponent* tail = pvl_data(pvl_tail(parser->components));

		snprintf(temp,sizeof(temp),"Cant parse as %s value in %s property. Removing entire property",
			icalvalue_kind_to_string(value_kind),
			icalproperty_kind_to_string(prop_kind));

		insert_error(tail, str, temp,
			     ICAL_XLICERRORTYPE_VALUEPARSEERROR);

		/* Remove the troublesome property */
		icalcomponent_remove_property(tail,prop);
		icalproperty_free(prop);
		prop = 0;
		tail = 0;
		parser->state = ICALPARSER_ERROR;
		icalmemory_free_buffer(str);
		return 0;
		    
	    } else {
		vcount++;
		icalproperty_set_value(prop, value);
		icalmemory_free_buffer(str);
	    }


	} else {
	    if (vcount == 0){
		char temp[200]; /* HACK */
		
		icalproperty_kind prop_kind = icalproperty_isa(prop);
		icalcomponent *tail = pvl_data(pvl_tail(parser->components));
		
		snprintf(temp,sizeof(temp),"No value for %s property. Removing entire property",
			icalproperty_kind_to_string(prop_kind));

		insert_error(tail, str, temp,
			     ICAL_XLICERRORTYPE_VALUEPARSEERROR);

		/* Remove the troublesome property */
		icalcomponent_remove_property(tail,prop);
		icalproperty_free(prop);
		prop = 0;
		tail = 0;
		parser->state = ICALPARSER_ERROR;
		return 0;
	    } else {

		break;
	    }
	}
    }
	
    /****************************************************************
     * End of component parsing. 
     *****************************************************************/

    if (pvl_data(pvl_tail(parser->components)) == 0 &&
	parser->level == 0){
	/* HACK. Does this clause ever get executed? */
	parser->state = ICALPARSER_SUCCESS;
	assert(0);
	return parser->root_component;
    } else {
	parser->state = ICALPARSER_IN_PROGRESS;
	return 0;
    }

}

icalparser_state icalparser_get_state(icalparser* parser)
{
    return parser->state;

}

icalcomponent* icalparser_clean(icalparser* parser)
{
    icalcomponent *tail; 

    icalerror_check_arg_rz((parser != 0 ),"parser");

    /* We won't get a clean exit if some components did not have an
       "END" tag. Clear off any component that may be left in the list */
    
    while((tail=pvl_data(pvl_tail(parser->components))) != 0){

	insert_error(tail," ",
		     "Missing END tag for this component. Closing component at end of input.",
		     ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	

	parser->root_component = pvl_pop(parser->components);
	tail=pvl_data(pvl_tail(parser->components));

	if(tail != 0){
	    if(icalcomponent_get_parent(parser->root_component)!=0){
		icalerror_warn("icalparser_clean is trying to attach a component for the second time");
	    } else {
		icalcomponent_add_component(tail,parser->root_component);
	    }
	}
	    
    }

    return parser->root_component;

}

struct slg_data {
	const char* pos;
	const char* str;
};


char* icalparser_string_line_generator(char *out, size_t buf_size, void *d)
{
    char *n;
    size_t size;
    struct slg_data* data = (struct slg_data*)d;

    if(data->pos==0){
	data->pos=data->str;
    }

    /* If the pointer is at the end of the string, we are done */
    if (*(data->pos)==0){
	return 0;
    }

    n = strchr(data->pos,'\n');
    
    if (n == 0){
	size = strlen(data->pos);
    } else {
	n++; /* include newline in output */
	size = (n-data->pos);	
    }

    if (size > buf_size-1){
	size = buf_size-1;
    }
    

    strncpy(out,data->pos,size);
    
    *(out+size) = '\0';
    
    data->pos += size;

    return out;    
}

icalcomponent* icalparser_parse_string(const char* str)
{
    icalcomponent *c;
    struct slg_data d;
    icalparser *p;

    icalerrorstate es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);

    d.pos = 0;
    d.str = str;

    p = icalparser_new();
    icalparser_set_gen_data(p,&d);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);

    c = icalparser_parse(p,icalparser_string_line_generator);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);

    icalparser_free(p);

    return c;

}
