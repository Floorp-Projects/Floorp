/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparser.h
  CREATOR: eric 20 April 1999
  
  $Id: icalparser.h,v 1.7 2002/06/28 08:55:23 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparser.h

======================================================================*/


#ifndef ICALPARSER_H
#define ICALPARSER_H

#include "icalenums.h"
#include "icaltypes.h"
#include"icalcomponent.h"

#include <stdio.h> /* For FILE* */

typedef struct icalparser_impl icalparser;


/**
 * @file  icalparser.h
 * @brief Line-oriented parsing. 
 * 
 * Create a new parser via icalparse_new_parser, then add lines one at
 * a time with icalparse_add_line(). icalparser_add_line() will return
 * non-zero when it has finished with a component.
 */

typedef enum icalparser_state {
    ICALPARSER_ERROR,
    ICALPARSER_SUCCESS,
    ICALPARSER_BEGIN_COMP,
    ICALPARSER_END_COMP,
    ICALPARSER_IN_PROGRESS
} icalparser_state;

icalparser* icalparser_new(void);
icalcomponent* icalparser_add_line(icalparser* parser, char* str );
icalcomponent* icalparser_clean(icalparser* parser);
icalparser_state icalparser_get_state(icalparser* parser);
void icalparser_free(icalparser* parser);


/**
 * Message oriented parsing.  icalparser_parse takes a string that
 * holds the text ( in RFC 2445 format ) and returns a pointer to an
 * icalcomponent. The caller owns the memory. line_gen_func is a
 * pointer to a function that returns one content line per invocation
 */

icalcomponent* icalparser_parse(icalparser *parser,
	char* (*line_gen_func)(char *s, size_t size, void *d));

/**
   Set the data that icalparser_parse will give to the line_gen_func
   as the parameter 'd'
 */
void icalparser_set_gen_data(icalparser* parser, void* data);


icalcomponent* icalparser_parse_string(const char* str);


/***********************************************************************
 * Parser support functions
 ***********************************************************************/

/** Use the flex/bison parser to turn a string into a value type */
icalvalue*  icalparser_parse_value(icalvalue_kind kind, 
				   const char* str, icalcomponent** errors);

/** Given a line generator function, return a single iCal content line.*/
char* icalparser_get_line(icalparser* parser, char* (*line_gen_func)(char *s, size_t size, void *d));

char* icalparser_string_line_generator(char *out, size_t buf_size, void *d);

#endif /* !ICALPARSE_H */
