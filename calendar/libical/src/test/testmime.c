/* -*- Mode: C -*-
  ======================================================================
  FILE: 
  CREATOR: eric 25 June 2000
  
  $Id: testmime.c,v 1.3 2002/06/28 10:45:12 acampi Exp $
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

#include "ical.h"

#include <stdlib.h> /* For rand */
#include <string.h> /* for strrchr, strdup*/
#include <unistd.h>   /* for getopt */

/*int sspm_parse_mime(struct sspm_part *parts, 
	       size_t max_parts,
	       struct sspm_action_map *actions,
	       char* (*get_string)(char *s, size_t size, void* data),
	       void *get_string_data,
	       struct sspm_header *first_header
    );
*/



char* major_type_string[] = {
    "TEXT",
    "IMAGE",
    "AUDIO",
    "VIDEO",
    "APPLICATION",
    "MULTIPART",
    "MESSAGE",
    "UNKNOWN",
    "NO"
};

char* minor_type_string[] = {
    "ANY",
    "PLAIN",
    "RFC822",
    "DIGEST",
    "CALENDAR",
    "MIXED",
    "RELATED",
    "ALTERNATIVE",
    "PARALLEL",
    "UNKNOWN",
    "NO"
};


char* read_stream(char *s, size_t size, void *d)
{
    char *c = fgets(s,size, (FILE*)d);

    return c;

}


int main(int argc, char* argv[]) {

    FILE *f;
    int c;
    extern char *optarg;
    extern int optind, optopt;
    int errflg=0;
    char* program_name;

    struct options{
	    int normal;
	    int stress;
	    int base64;
	    int qp;
	    int sleep;
	    int count;
	    char* input_file;
    } opt;

    memset(&opt,0,sizeof(opt));
    
    program_name = (char*)strrchr((char*)argv[0],'/');
    program_name++;

    while ((c = getopt(argc, argv, "nsbqi:S:c:")) != -1) {
	switch (c) {
	    case 'i': { /* Input comes from named file */
		opt.input_file = strdup(optarg);
		break;
	    }
	    case 'n':{ /* Normal */

		if(opt.stress+opt.base64+opt.qp != 0){
		    fprintf(stderr,
			    "%s: Use only one of  n,s,b and q\n",
			    program_name);
		}
		opt.normal = 1;
		break;
	    }
	    case 's':{ /* Stress-test*/
		if(opt.base64+opt.normal+opt.qp != 0){
		    fprintf(stderr,
			    "%s: Use only one of  n,s,b and q\n",
			    program_name);
		}
		opt.stress = 1;
		break;
	    }
	    case 'b':{ /* test base64 decoding*/
		if(opt.stress+opt.normal+opt.qp != 0){
		    fprintf(stderr,
			    "%s: Use only one of  n,s,b and q\n",
			    program_name);
		}
		opt.base64 = 1;
		break;
	    }
	    case 'q':{ /* test quoted-printable decoding*/
		if(opt.stress+opt.base64+opt.normal != 0){
		    fprintf(stderr,
			    "%s: Use only one of  n,s,b and q\n",
			    program_name);
		}
		opt.qp = 1;
		break;
	    }
	    case 'S':{ /* sleep at end of run */
		opt.sleep = atoi(optarg);
		break;
	    }

	    case 'c':{ /* number of iterations of stress test */
		opt.count = atoi(optarg);
		break;
	    }
	    
	    case ':': {/* Option given without an operand */
		fprintf(stderr,
			"%s: Option -%c requires an operand\n", 
			program_name,optopt);
		errflg++;
		break;
	    }
	    case '?': {
		errflg++;
	    }
	}
    }
    
    if (errflg >0){
	fprintf(stderr,"Usage: %s [-n|-s|-b|-q] [-i input_file]\n",
		program_name);
	exit(1);
    }
    
    if(opt.stress+opt.base64+opt.normal+opt.qp == 0){
	fprintf(stderr,
		"%s: Must have one of n,s,b or q\n",
		program_name);
    }
    
    if(opt.input_file){
	f = fopen(opt.input_file,"r");
	if (f == 0){
	    fprintf(stderr,"Could not open input file \"%s\"\n",
		    opt.input_file);
	    exit(1);
	}
    } else {
	f = stdin;
    }
	
 	

    if(opt.normal == 1){
	icalcomponent *c;

	c = icalmime_parse(read_stream,f);
	
	printf("%s\n",icalcomponent_as_ical_string(c));

	icalcomponent_free(c);

    }  else if (opt.stress==1 ){
	/* Read file in by lines, then randomize the lines into a
           string buffer */

	char *array[1024];
	char temp[1024];
       	char *buf;
	int i,last;
	int size;
	int non_rand;
	int rand_lines;
	int r;
	int j;
	icalcomponent *c;
	struct slg_data {
		char* pos;
		char* str;
	} d;
    
	for(i=0; !feof(f); i++){
	    fgets(temp,1024,f);
	    array[i] = strdup(temp);
	    size += strlen(temp);
	}
	last = i;
 
	buf = malloc(size*2);
	assert(buf != 0);
	

	for(j=0; j<opt.count; j++){

	    srand(j);
	    memset(buf,0,size*2);
	    /* First insert some non-randomized lines */
	    non_rand = ((float)rand()/(float)RAND_MAX) * last;
	    for(i=0;i<non_rand;i++){
		strcat(buf,array[i]);
	    }
	    
	    /* Then, insert some lines at random */
	    
	    rand_lines = last - non_rand;
	    
	    for(i=0;i<rand_lines;i++){
		srand(i);
		r = ((float)rand()/(float)RAND_MAX) * rand_lines;
		strcat(buf,array[r+non_rand]);
		
	    }

	    d.pos = 0;
	    d.str = buf;

	    c = icalmime_parse(icalparser_string_line_generator,&d);

	    printf("%s\n",icalcomponent_as_ical_string(c));

	    icalcomponent_free(c);
	    
	}

	free(buf);

	for(i=0; i<last; i++){
	    free(array[i]);
	}

    } else if(opt.qp == 1){	
	char str[4096];
	char conv[4096];
	
	memset(str,0,4096);
	
	while(!feof(f) && fgets(str,4096,f)!=0){
	    size_t size;
	    
	    size = strlen(str);
	    memset(conv,0,4096);
	    decode_quoted_printable(conv,str,&size);
	    
	    conv[size] = '\0';
	    printf("%s",conv);
	    memset(str,0,4096);
	    
	}
    } else if (opt.base64 == 1) {
	char str[4096];
	char conv[4096];
	
	memset(str,0,4096);
	
	while(!feof(f) && fgets(str,4096,f)!=0){
	    size_t size;
	    
	    size = strlen(str);
	    memset(conv,0,4096);
	    decode_base64(conv,str,&size);
	    
	    conv[size] = '\0';
	    printf("%s",conv);
	    memset(str,0,4096);
	    
	}
    }

    if (opt.sleep != 0){
	sleep(opt.sleep);
    }

    if(	opt.input_file != 0){
	free(opt.input_file);
    }

    icalmemory_free_ring();

    return 0;

}





	
	
       
