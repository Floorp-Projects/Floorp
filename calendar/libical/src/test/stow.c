/* -*- Mode: C -*-
  ======================================================================
  FILE: stow.c
  CREATOR: eric 29 April 2000
  
  $Id: stow.c,v 1.8 2003/11/17 22:51:53 gray-john Exp $
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
 
 The Initial Developer of the Original Code is Eric Busboom

 ======================================================================*/


#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> /* for PATH_MAX */
#include <assert.h>
#include <stdlib.h>
#include <sys/utsname.h> /* for uname */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat, getpid, getopt */
#include <pwd.h> /* For getpwent */
#include <sys/types.h> /* For getpwent */
#include <ctype.h> /* for tolower */

#include "ical.h"
#include "icalss.h"

char* program_name;
#define TMPSIZE 2048
#define SENDMAIL "/usr/lib/sendmail -t"

void usage(char *message);

#ifndef PATH_MAX
#define PATH_MAX 256 /* HACK */
#endif


enum options {
    STORE_IN_FILE,
    STORE_IN_DB,
    INPUT_IS_MIME,
    INPUT_IS_ICAL,
    INPUT_FROM_STDIN,
    INPUT_FROM_FILE,
    ERRORS_TO_STDOUT,
    ERRORS_TO_ORGANIZER
};

struct options_struct
{
	enum options storage;
	enum options input_type;
	enum options input_source;
	enum options errors;
	char* input_file;
	char* calid;
	char* output_file;
};


enum file_type
{
    ERROR,
    NO_FILE,
    DIRECTORY,
    REGULAR,
    OTHER
};

enum file_type test_file(char *path)
{
    struct stat sbuf;
    enum file_type type;
    
    errno = 0;

    /* Check if the path already exists and if it is a directory*/
    if (stat(path,&sbuf) != 0){
	
	/* A file by the given name does not exist, or there was
           another error */
	if(errno == ENOENT)
	{
	    type = NO_FILE;
	} else {
	    type = ERROR;
	}

    } else {
	/* A file by the given name exists, but is it a directory? */
	
	if (S_ISDIR(sbuf.st_mode)){ 
	    type = DIRECTORY;
	} else if(S_ISREG(sbuf.st_mode)){ 
	    type = REGULAR;
	} else {
	    type = OTHER;
	}
    }

    return type;
}

char* lowercase(const char* str)
{
    char* p = 0;
    char* ret;

    if(str ==0){
	return 0;
    }

    ret = strdup(str);

    for(p = ret; *p!=0; p++){
	*p = tolower(*p);
    }

    return ret;
}

#if 0
char* get_local_attendee(struct options_struct *opt)
{
    char attendee[PATH_MAX];

    if(opt->calid){

	strncpy(attendee,opt->calid,PATH_MAX);

    } else {
	
	char* user = getenv("USER");
	struct utsname uts;
	uname(&utget_option);
	/* HACK nodename may not be a fully qualified domain name */
	snprintf(attendee,PATH_MAX,"%s@%s",user,uts.nodename);
   
    }

    return lowercase(attendee);
}
#endif


icalcomponent* get_first_real_component(icalcomponent *comp)
{
    icalcomponent *c;

    for(c = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){
	if (icalcomponent_isa(c) == ICAL_VEVENT_COMPONENT ||
	    icalcomponent_isa(c) == ICAL_VTODO_COMPONENT ||
	    icalcomponent_isa(c) == ICAL_VJOURNAL_COMPONENT )
	{
	    return c;
	}
    }

    return 0;
}



char* make_mime(const char* to, const char* from, const char* subject, 
		const char* text_message, const char* method, 
		const char* ical_message)
{
    size_t size = strlen(to)+strlen(from)+strlen(subject)+
	strlen(text_message)+ strlen(ical_message)+TMPSIZE;

    char mime_part_1[TMPSIZE];
    char mime_part_2[TMPSIZE];
    char content_id[TMPSIZE];
    char boundary[TMPSIZE];
    struct utsname uts;
    char* m;


    if ((m = malloc(sizeof(char)*size)) == 0){
	fprintf(stderr,"%s: Can't allocate memory: %s\n",program_name,strerror(errno));
	exit(1);
    }

    uname(&uts);

    srand(time(0)<<getpid());
    sprintf(content_id,"%d-%d@%s",(int)time(0),rand(),uts.nodename);
    sprintf(boundary,"%d-%d-%s",(int)time(0),rand(),uts.nodename);

    sprintf(mime_part_1,"Content-ID: %s\n\
Content-type: text/plain\n\
Content-Description: Text description of error message\n\n\
%s\n\n--%s",
	    content_id,text_message,boundary);

    if(ical_message != 0 && method != 0){
	sprintf(mime_part_2,"Content-ID: %s\n\
Content-type: text/calendar; method=%s\n\
Content-Description: iCal component reply\n\n\
%s\n\n--%s--",
		content_id,method,ical_message,boundary);
    }

    sprintf(m,"To: %s\n\
From: %s\n\
Subject: %s\n\
MIME-Version: 1.0\n\
Content-ID: %s\n\
Content-Type:  multipart/mixed; boundary=\"%s\"\n\
\n\
 This is a multimedia message in MIME format\n\
\n\
--%s\n\
%s\n\
",
	    to,from,subject,content_id,boundary,boundary,
	    mime_part_1);

    if(ical_message != 0 && method != 0){
	strcat(m, mime_part_2);
    } else {
	strcat(m,"--\n");
    }

    return m;
}

/* The incoming component had fatal errors */
void return_failure(icalcomponent* comp,  char* message, 
		    struct options_struct *opt)
{
    char* local_attendee = opt->calid;
    FILE* p;
    const char *org_addr;

    icalcomponent  *inner = get_first_real_component(comp);

    icalproperty *organizer_prop = icalcomponent_get_first_property(inner,ICAL_ORGANIZER_PROPERTY);
    const char *organizer = icalproperty_get_organizer(organizer_prop);

    org_addr = strchr(organizer,':');

    if(org_addr != 0){
        org_addr++; /* Skip the ';' */
    } else {
        org_addr = organizer;
    }

    if (opt->errors == ERRORS_TO_ORGANIZER){
	p = popen(SENDMAIL,"w");
    } else {
	p = stdout;
    }

    if(p == 0){
	fprintf(stderr,
		"%s: fatal. Could not open pipe to sendmail (\"%s\") \n",
		program_name,SENDMAIL);
	exit(1);
     }
   
    fputs(make_mime(org_addr, local_attendee, "iMIP error", 
		    message, "reply",
		    icalcomponent_as_ical_string(comp)),p);
    
    if (opt->errors == ERRORS_TO_ORGANIZER){ 
	pclose(p);
    }   
}

/* The program had a fatal error and could not process the incoming component*/
void return_error(icalcomponent* comp,  char* message, struct options_struct *opt)
{
    

    fputs(make_mime("Dest", "Source", "iMIP system failure", 
		    message, 0,0),stdout);

}

icalcomponent* make_reply(icalcomponent *comp, icalproperty *return_status, 
			 struct options_struct *opt)

{
    icalcomponent *reply, *rinner;
    icalcomponent  *inner = get_first_real_component(comp);
    icalproperty *p=0;
    char* local_attendee = opt->calid;
    char attendee[TMPSIZE];

    char prodid[TMPSIZE];

    snprintf(attendee,TMPSIZE,"mailto:%s",local_attendee);

    snprintf(prodid,TMPSIZE,"-//Softwarestudio.org//%s version %s//EN",ICAL_PACKAGE,ICAL_VERSION);

    /* Create the base component */
    reply = icalcomponent_vanew( 
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_version(strdup("2.0")),
	icalproperty_new_prodid(strdup(prodid)),
	icalproperty_new_method(ICAL_METHOD_REPLY),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_clone(
		icalcomponent_get_first_property(inner,ICAL_DTSTAMP_PROPERTY)),
	    icalproperty_new_clone(
		icalcomponent_get_first_property(inner,ICAL_ORGANIZER_PROPERTY)),
	    icalproperty_new_clone(
		icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY)),
	    icalproperty_new_attendee(attendee),
	    0),
	0);

    
    /* Convert errors into request-status properties and transfers
       them to the reply component */

    icalcomponent_convert_errors(comp);

    rinner = get_first_real_component(reply);

    for(p = icalcomponent_get_first_property(inner,
					     ICAL_REQUESTSTATUS_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(inner,
					    ICAL_REQUESTSTATUS_PROPERTY)){
	
	icalcomponent_add_property(rinner,icalproperty_new_clone(p));
    }
		
    if(return_status != 0){
	icalcomponent_add_property(rinner, return_status);
    }
        
    return reply;
	    
}

int check_attendee(icalproperty *p,  struct options_struct *opt){  
    const char* s = icalproperty_get_attendee(p);
    char* lower_attendee = lowercase(s);
    char* local_attendee = opt->calid;
    
    /* Check that attendee begins with "mailto:" */
    if (strncmp(lower_attendee,"mailto:",7) == 0){
	/* skip over the mailto: part */
	lower_attendee += 7;
	
	if(strcmp(lower_attendee,local_attendee) == 0){
	    return 1;
	}
	
	lower_attendee -= 7;
	
	free(lower_attendee);
    } 

    return 0;
}

char static_component_error_str[PATH_MAX];
char* check_component(icalcomponent* comp,  icalproperty **return_status,
		      struct options_struct *opt)
{
    char* component_error_str=0;
    icalcomponent* inner;
    int errors = 0;
    icalproperty *p;
    int found_attendee = 0;
    struct icalreqstattype rs;

    rs.code =  ICAL_UNKNOWN_STATUS;
    rs.desc = 0;
    rs.debug = 0;

    /*{
	icalrequeststatus code;
	const char* desc;
	const char* debug;
        };*/

    *return_status = 0;

    /* This do/while loop only executes once because it is being used
       to fake exceptions */

    do {

	/* Check that we actually got a component */
	if(comp == 0){
	    strcpy(static_component_error_str,
		   "Did not find a component");
	    component_error_str = static_component_error_str;
	    break;
	}

	/* Check that the root component is a VCALENDAR */
	if(icalcomponent_isa(comp) != ICAL_VCALENDAR_COMPONENT){
	    strcpy(static_component_error_str,
		   "Root component is not a VCALENDAR");
	    component_error_str = static_component_error_str;
            rs.code = ICAL_3_11_MISSREQCOMP_STATUS;

	    break;
	}


	/* Check that the component has a METHOD */

	if (icalcomponent_get_first_property(comp,ICAL_METHOD_PROPERTY) == 0)
	{
	    strcpy(static_component_error_str,
		   "The component you sent did not have a METHOD property");
	    component_error_str = static_component_error_str;
            rs.code = ICAL_3_11_MISSREQCOMP_STATUS;
	    break;
	}
	
	inner = get_first_real_component(comp);


	/* Check that the compopnent has an organizer */
	if(icalcomponent_get_first_property(inner,ICAL_ORGANIZER_PROPERTY) == 0){
	    fprintf(stderr,"%s: fatal. Component does not have an ORGANIZER property\n",program_name);
            rs.code = ICAL_3_11_MISSREQCOMP_STATUS;
            break;
	}


	/* Check for this user as an attendee or organizer */

	for(p = icalcomponent_get_first_property(inner,ICAL_ATTENDEE_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(inner,ICAL_ATTENDEE_PROPERTY)){
	  
	    found_attendee += check_attendee(p,opt);
	}

	for(p = icalcomponent_get_first_property(inner,ICAL_ORGANIZER_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(inner,ICAL_ORGANIZER_PROPERTY)){
	  
	    found_attendee += check_attendee(p,opt);
	}
		
	if (found_attendee == 0){
	    struct icalreqstattype rs;
	    memset(static_component_error_str,0,PATH_MAX);

	    snprintf(static_component_error_str,PATH_MAX,
		   "This target user (%s) is not listed as an attendee or organizer",
		    opt->calid );
	    component_error_str = static_component_error_str;

	    rs.code = ICAL_3_7_INVCU_STATUS;

	    break;
	}



	/* Check that the component passes iTIP restrictions */
	
	errors = icalcomponent_count_errors(comp);
	icalrestriction_check(comp);
	
	if(errors != icalcomponent_count_errors(comp)){
	    snprintf(static_component_error_str,PATH_MAX,
		   "The component does not conform to iTIP restrictions.\n Here is the original component; look at the X-LIC-ERROR properties\nfor details\n\n%s",icalcomponent_as_ical_string(comp));
	    component_error_str = static_component_error_str;
	    break;
	}



    } while(0);

    if(rs.code != ICAL_UNKNOWN_STATUS){
        *return_status = icalproperty_new_requeststatus(rs);
    }

    return component_error_str;
}


void usage(char *message)
{
    fprintf(stderr,"Usage: %s [-emdcn] [-i inputfile] [-o outputfile] [-u calid]\n",program_name);
    fprintf(stderr,"-e\tInput data is encapsulated in a MIME Message \n\
-m\tInput is raw iCal \n\
-i\tSpecify input file. Otherwise, input comes from stdin\n\
-o\tSpecify file to save incoming message to\n\
-d\tSpecify database to send data to\n\
-u\tSet the calid to store the data to\n\
-n\tSend errors to stdout instead of organizer\n\
");

}


void get_options(int argc, char* argv[], struct options_struct *opt)
{
    int c;
    extern char *optarg;
    extern int optind, optopt;
    int errflg=0;

    opt->storage = STORE_IN_FILE;
    opt->input_source = INPUT_FROM_STDIN;
    opt->input_type = INPUT_IS_ICAL;
    opt->input_file = 0;
    opt->errors = ERRORS_TO_ORGANIZER;
    opt->calid = 0;
    opt->output_file = 0;


    while ((c = getopt(argc, argv, "nemu:o:d:b:c:i:")) != -1) {
             switch (c) {
		 case 'e': { /* Input data is MIME encapsulated */
		     opt->input_type = INPUT_IS_MIME;
		     break;
		 }
		 case 'm': { /* Input is iCal. Default*/
		     opt->input_type = INPUT_IS_ICAL;
		     break;
		 }
		 case 'i': { /* Input comes from named file */
		     opt->input_source = INPUT_FROM_FILE;
		     opt->input_file = strdup(optarg);
		     break;
		 }
		 case 'o': { /* Output goes to named file. Default*/
		     opt->output_file = strdup(optarg);
		     opt->storage = STORE_IN_FILE;
		     break;
		 }
		 case 'd': { /* Output goes to database */
		     fprintf(stderr,"%s: option -d is unimplmented\n",program_name);
		     opt->storage = STORE_IN_DB;
		     errflg++;
		     break;
		 }
		 case 'c': {

		     break;
		 }
		 case 'u': { /* Set the calid for the output database or
				file. Default is user name of user running
				program */
		     opt->calid = strdup(optarg);
		     break;
		 }

		 case 'n': { /* Dump error to stdout. Default is to
		                send error to the organizer specified
		                in the iCal data */
		     opt->errors = ERRORS_TO_STDOUT;
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

	     if (errflg >0){
		 usage("");
		 exit(1);
	     }
    } 

    if(opt->calid == 0){
	/* If no calid specified, use username */
	char attendee[PATH_MAX];
	char* user = getenv("USER");
	struct utsname uts;
	uname(&uts);
	/* HACK nodename may not be a fully qualified domain name */
	snprintf(attendee,PATH_MAX,"%s@%s",user,uts.nodename);

	opt->calid = lowercase(attendee);
    }

    if(opt->storage == STORE_IN_FILE &&
	opt->output_file ==0){
	char file[PATH_MAX];
	char* user = getenv("USER");
	struct passwd *pw;

	if(!user){
	    fprintf(stderr,"%s: Can't get username. Try explicitly specifing the output file with -o", program_name);
	    exit(1);
	}
	
	/* Find password entry for user */
	while( (pw = getpwent())!=0){
	    if(strcmp(user,pw->pw_name)==0){
		break;
	    }
	}
	    
	if(pw==0){
	    fprintf(stderr,"%s: Can't get get password entry for user \"%s\" Try explicitly specifing the output file with -o", 
		    program_name,user);
	    exit(1);
	}

	if(pw->pw_dir==0){
	    fprintf(stderr,"%s: User \"%s\" has no home directory. Try explicitly specifing the output file with -o", 
		    program_name, user);
	    exit(1);
	}

	snprintf(file,PATH_MAX,"%s/.facs/%s",pw->pw_dir,opt->calid);

	opt->output_file = strdup(file);
    }


    /* Now try to create the calendar directory if it does
       not exist */

     if(opt->storage == STORE_IN_FILE ) {
	char * p;
	char* facspath = strdup(opt->output_file);
	enum file_type type;

	/* Cut off the last slash to make it just a directoy */

	p = strrchr(facspath,'/');

        if (p != 0){
            /* Use some other directory */
            *p='\0';
            
            type = test_file(facspath);
            
            errno = 0;
            if (type == NO_FILE){
                
                if(mkdir(facspath,0775) != 0){
                    fprintf(stderr,
                            "%s: Failed to create calendar directory %s: %s\n",
                            program_name,facspath, strerror(errno));
                    exit(1);
                } else {
                    fprintf(stderr,"%s: Creating calendar directory %s\n",
                            program_name,facspath);
                }
                
            } else if(type==REGULAR || type == ERROR){
                fprintf(stderr,"%s: Cannot create calendar directory %s\n",
                        program_name,facspath);
                exit(1);
            } 		    
        }
     }
}

char* check_options(struct options_struct *opt)
{
    return 0;
}

void store_component(icalcomponent *comp, struct options_struct *opt)
{
    icalerrorenum error; 


    if(opt->storage == STORE_IN_FILE){
	icalset *fs = icalfileset_new(opt->output_file);

	if (fs == 0){
	    fprintf(stderr,
		    "%s: Failed to get incoming component directory: %s\n",
		    program_name, icalerror_strerror(icalerrno));
	    exit(1);
	}


	error = icalfileset_add_component(fs,comp);
    
	if (error != ICAL_NO_ERROR){
	    fprintf(stderr,"%s: Failed to write incoming component: %s\n",
		    program_name, icalerror_strerror(icalerrno));
	    exit(1);
	}
	
	error = icalfileset_commit(fs);
    
	if (error != ICAL_NO_ERROR){
	    fprintf(stderr,"%s: Failed to commit incoming cluster: %s\n",
		    program_name, icalerror_strerror(icalerrno));
	    exit(1);
	}
	
	icalset_free(fs);

	return;
    } else {
	assert(0);
    }
}

char* read_stream(char *s, size_t size, void *d)
{
  char *c = fgets(s,size, (FILE*)d);

  return c;
}

icalcomponent* read_nonmime_component(struct options_struct *opt)
{
    FILE *stream;
    icalcomponent *comp;
    icalparser* parser = icalparser_new();
    icalerrorstate es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
    char* line;

    if(opt->input_source == INPUT_FROM_FILE){
 	stream = fopen(opt->input_file,"r"); 	

	if (stream == 0){
	    perror("Can't open input file");
	    exit(1);
	}

    } else {
 	stream = stdin;  	 	
    }

    assert(stream != 0);
    icalparser_set_gen_data(parser,stream);
   
    do {	
	line = icalparser_get_line(parser,read_stream);
	
        icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);
	comp = icalparser_add_line(parser,line);
        icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
	
	if (comp != 0){
	    return comp;
	}
	
    } while ( line != 0);

    if(opt->input_source == INPUT_FROM_FILE){
	fclose(stream);
    }


    return comp;
 }

icalcomponent* find_vcalendar(icalcomponent* comp)
{
    icalcomponent *c,*rtrn;

    for(c = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){

	if(icalcomponent_isa(c) == ICAL_VCALENDAR_COMPONENT){
	    icalcomponent_remove_component(comp,c);
	    return c;
	}

	if((rtrn=find_vcalendar(c)) != 0){
	    return rtrn;
	}
    }

    return 0;
}

icalcomponent* read_mime_component(struct options_struct *opt)
{
    icalcomponent *comp,*mimecomp;
    FILE* stream;
    
    if(opt->input_source == INPUT_FROM_FILE){
 	stream = fopen(opt->input_file,"r"); 	
	
	if (stream == 0){
	    perror("Can't open input file");
	    exit(1);
	}
	
    } else {
 	stream = stdin;  	 	
    }
    
    assert(stream != 0);
    
    mimecomp = icalmime_parse(read_stream,(void*)stream);

    /* now find the iCal component embedded within the mime component */
    comp = find_vcalendar(mimecomp);


    if(comp == 0){
	return 0;
    }

    return comp;    
}

icalcomponent* read_component(struct options_struct *opt)
{
    if(opt->input_type == INPUT_IS_MIME){
	return read_mime_component(opt);
    } else if (opt->input_type == INPUT_IS_ICAL){
	return read_nonmime_component(opt);
    } else {
	fprintf(stderr,"%s: Internal Error; unknown option for input_type\n",
		program_name);
	exit(1);
    }
}

int main(int argc, char* argv[] )
{
    char* options_error_str;
    char* component_error_str;
    icalcomponent* comp, *reply;
    struct options_struct opt;
    icalproperty *return_status;

    program_name = strrchr(argv[0],'/');

    get_options(argc, argv, &opt);

    if ( (options_error_str = check_options(&opt)) != 0 ){
	usage(options_error_str);
	exit(1);
    }    

    comp = read_component(&opt);

    /* If the component had any fatal errors, return an error message
       to the organizer */
    if ( (component_error_str = 
	  check_component(comp,&return_status,&opt)) != 0){

	reply = make_reply(comp,return_status,&opt);

	return_failure(reply, component_error_str, &opt);
	icalcomponent_free(reply);
	exit(0);

    }

    store_component(comp,&opt);


    /* Don't free the component comp, since it is now part of the
       store, and will be freed there */

    exit(0);
}

