/* -*- Mode: C -*-
  ======================================================================
  FILE: sspm.h Mime Parser
  CREATOR: eric 25 June 2000
  
  $Id: sspm.h,v 1.2 2001/01/23 07:03:17 ebusboom Exp $
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

#ifndef SSPM_H
#define SSPM_H

enum sspm_major_type {
    SSPM_NO_MAJOR_TYPE,
    SSPM_TEXT_MAJOR_TYPE,
    SSPM_IMAGE_MAJOR_TYPE,
    SSPM_AUDIO_MAJOR_TYPE,
    SSPM_VIDEO_MAJOR_TYPE,
    SSPM_APPLICATION_MAJOR_TYPE,
    SSPM_MULTIPART_MAJOR_TYPE,
    SSPM_MESSAGE_MAJOR_TYPE,
    SSPM_UNKNOWN_MAJOR_TYPE
};

enum sspm_minor_type {
    SSPM_NO_MINOR_TYPE,
    SSPM_ANY_MINOR_TYPE,
    SSPM_PLAIN_MINOR_TYPE,
    SSPM_RFC822_MINOR_TYPE,
    SSPM_DIGEST_MINOR_TYPE,
    SSPM_CALENDAR_MINOR_TYPE,
    SSPM_MIXED_MINOR_TYPE,
    SSPM_RELATED_MINOR_TYPE,
    SSPM_ALTERNATIVE_MINOR_TYPE,
    SSPM_PARALLEL_MINOR_TYPE,
    SSPM_UNKNOWN_MINOR_TYPE
};

enum sspm_encoding {
    SSPM_NO_ENCODING,
    SSPM_QUOTED_PRINTABLE_ENCODING,
    SSPM_8BIT_ENCODING,
    SSPM_7BIT_ENCODING,
    SSPM_BINARY_ENCODING,
    SSPM_BASE64_ENCODING,
    SSPM_UNKNOWN_ENCODING
};

enum sspm_error{
    SSPM_NO_ERROR,
    SSPM_UNEXPECTED_BOUNDARY_ERROR,
    SSPM_WRONG_BOUNDARY_ERROR,
    SSPM_NO_BOUNDARY_ERROR,
    SSPM_NO_HEADER_ERROR,
    SSPM_MALFORMED_HEADER_ERROR
};


struct sspm_header
{
	int def;
	char* boundary;
	enum sspm_major_type major;
	enum sspm_minor_type minor;
	char *minor_text;
	char ** content_type_params;
	char* charset;
	enum sspm_encoding encoding;
	char* filename;
	char* content_id;
	enum sspm_error error;
	char* error_text;
};

struct sspm_part {
	struct sspm_header header;
	int level;
	size_t data_size;
	void *data;
};

struct sspm_action_map {
	enum sspm_major_type major;
	enum sspm_minor_type minor;
	void* (*new_part)();
	void (*add_line)(void *part, struct sspm_header *header, 
			 char* line, size_t size);
	void* (*end_part)(void* part);
	void (*free_part)(void *part);
};

char* sspm_major_type_string(enum sspm_major_type type);
char* sspm_minor_type_string(enum sspm_minor_type type);
char* sspm_encoding_string(enum sspm_encoding type);

int sspm_parse_mime(struct sspm_part *parts, 
		    size_t max_parts,
		    struct sspm_action_map *actions,
		    char* (*get_string)(char *s, size_t size, void* data),
		    void *get_string_data,
		    struct sspm_header *first_header
    );

void sspm_free_parts(struct sspm_part *parts, size_t max_parts);

char *decode_quoted_printable(char *dest, 
				       char *src,
				       size_t *size);
char *decode_base64(char *dest, 
			     char *src,
			     size_t *size);


int sspm_write_mime(struct sspm_part *parts,size_t num_parts,
		    char **output_string, char* header);

#endif /*SSPM_H*/
