/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalattach.h
 CREATOR: acampi 28 May 02


 (C) COPYRIGHT 2002, Andrea Campi

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalattach.h

======================================================================*/

#ifndef ICALATTACH_H
#define ICALATTACH_H


typedef struct icalattach_impl icalattach;

typedef void (* icalattach_free_fn_t) (unsigned char *data, void *user_data);

icalattach *icalattach_new_from_url (const char *url);
icalattach *icalattach_new_from_data (unsigned char *data,
	icalattach_free_fn_t free_fn, void *free_fn_data);

void icalattach_ref (icalattach *attach);
void icalattach_unref (icalattach *attach);

int icalattach_get_is_url (icalattach *attach);
const char *icalattach_get_url (icalattach *attach);
unsigned char *icalattach_get_data (icalattach *attach);

struct icalattachtype* icalattachtype_new(void);
void  icalattachtype_add_reference(struct icalattachtype* v);
void icalattachtype_free(struct icalattachtype* v);

void icalattachtype_set_url(struct icalattachtype* v, char* url);
char* icalattachtype_get_url(struct icalattachtype* v);

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns);
char* icalattachtype_get_base64(struct icalattachtype* v);

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns);
void* icalattachtype_get_binary(struct icalattachtype* v);



#endif /* !ICALATTACH_H */
