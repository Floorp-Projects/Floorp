/* -*- Mode: C -*-
  ======================================================================
  FILE: icallangbind.c
  CREATOR: eric 15 dec 2000
  
  DESCRIPTION:
  
  $Id: icallangbind.c,v 1.22 2002/10/24 13:44:30 acampi Exp $
  $Locker:  $

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org
  
  This package is free software and is provided "as is" without
  express or implied warranty.  It may be used, redistributed and/or
  modified under the same terms as perl itself. ( Either the Artistic
  License or the GPL. )

  ======================================================================*/

#include "icalcomponent.h"
#include "icalproperty.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalvalue.h"
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

int* icallangbind_new_array(int size){
    int* p = (int*)malloc(size*sizeof(int));
    return p; /* Caller handles failures */
}

void icallangbind_free_array(int* array){
    free(array);
}

int icallangbind_access_array(int* array, int index) {
    return array[index];
}                    

/** Iterators to fetch parameters given property */

icalparameter* icallangbind_get_first_parameter(icalproperty *prop)

{
    icalparameter_kind kind = ICAL_ANY_PARAMETER;
    
    return icalproperty_get_first_parameter(prop,kind);
}

icalparameter* icallangbind_get_next_parameter(icalproperty *prop)
{
    icalparameter_kind kind = ICAL_ANY_PARAMETER;
    
    return icalproperty_get_next_parameter(prop,kind);
}
		                                              

/** Like icalcomponent_get_first_component(), but takes a string for the
   kind and can iterate over X properties as if each X name was a
   seperate kind */

icalproperty* icallangbind_get_first_property(icalcomponent *c,
                                              const char* prop)
{
    icalproperty_kind kind = icalproperty_string_to_kind(prop);
    icalproperty *p;

    if (kind == ICAL_NO_PROPERTY){
	return 0;
    }

    if(kind == ICAL_X_PROPERTY){
        for(p = icalcomponent_get_first_property(c,kind);
            p !=0;
            p = icalcomponent_get_next_property(c,kind)){
            
            if(strcmp(icalproperty_get_x_name(p),prop) == 0){
                return p;
            }                
        }
    } else {
        p=icalcomponent_get_first_property(c,kind);

        return p;
    }
	
    return 0;

}

icalproperty* icallangbind_get_next_property(icalcomponent *c,
                                              const char* prop)
{
    icalproperty_kind kind = icalenum_string_to_property_kind(prop);
    icalproperty *p;

    if (kind == ICAL_NO_PROPERTY){
	return 0;
    }

    if(kind == ICAL_X_PROPERTY){
        for(p = icalcomponent_get_next_property(c,kind);
            p !=0;
            p = icalcomponent_get_next_property(c,kind)){
            
            if(strcmp(icalproperty_get_x_name(p),prop) == 0){
                return p;
            }                
        }
    } else {
        p=icalcomponent_get_next_property(c,kind);

        return p;
    }
	
    return 0;

}


icalcomponent* icallangbind_get_first_component(icalcomponent *c,
                                              const char* comp)
{
    icalcomponent_kind kind = icalenum_string_to_component_kind(comp);

    if (kind == ICAL_NO_COMPONENT){
	return 0;
    }
    return icalcomponent_get_first_component(c,kind);
}

icalcomponent* icallangbind_get_next_component(icalcomponent *c,
                                              const char* comp)
{
    icalcomponent_kind kind = icalenum_string_to_component_kind(comp);

    if (kind == ICAL_NO_COMPONENT){
	return 0;
    }
    return icalcomponent_get_next_component(c,kind);
}


#define APPENDS(x) icalmemory_append_string(&buf, &buf_ptr, &buf_size, x);

#define APPENDC(x) icalmemory_append_char(&buf, &buf_ptr, &buf_size, x);


const char* icallangbind_property_eval_string(icalproperty* prop, char* sep)
{
    char tmp[25];
    size_t buf_size = 1024;
    char* buf = icalmemory_new_buffer(buf_size);
    char* buf_ptr = buf;
    icalparameter *param;
    
    icalvalue* value;

    if( prop == 0){
	return 0;
    }

    APPENDS("{ ");

    value = icalproperty_get_value(prop);

    APPENDS(" 'name' ");
    APPENDS(sep);
    APPENDC('\'');
    APPENDS(icalproperty_kind_to_string(icalproperty_isa(prop)));
    APPENDC('\'');

    if(value){
        APPENDS(", 'value_type' ");
        APPENDS(sep);
        APPENDC('\'');
        APPENDS(icalvalue_kind_to_string(icalvalue_isa(value)));
        APPENDC('\'');
    }

    APPENDS(", 'pid' ");
    APPENDS(sep);
    APPENDC('\'');
    snprintf(tmp,25,"%p",prop);
    APPENDS(tmp);
    APPENDC('\'');


    if(value){
        switch (icalvalue_isa(value)){
	
        case ICAL_ATTACH_VALUE:
        case ICAL_BINARY_VALUE: 
        case ICAL_NO_VALUE: {
            icalerror_set_errno(ICAL_INTERNAL_ERROR);
            break;
        }

        default: 
        {
            const char* str = icalvalue_as_ical_string(value);
            char* copy = (char*) malloc(strlen(str)+1);
            
            const char *i;
            char *j;

            if(copy ==0){
                icalerror_set_errno(ICAL_NEWFAILED_ERROR);
                break; 
            }
            /* Remove any newlines */
                
            for(j=copy, i = str; *i != 0; j++,i++){
                if(*i=='\n'){
                    i++;
                }   
                *j = *i;
            }
                
            *j = 0;
                
            APPENDS(", 'value'");
            APPENDS(sep);
            APPENDC('\'');
            APPENDS(copy);
            APPENDC('\'');
            
            free(copy);
            break;

        }
        }
    }

    /* Add Parameters */

    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
        param != 0;
        param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)){
        
        const char* str = icalparameter_as_ical_string(param);
        char *copy = icalmemory_tmp_copy(str);
        char *v;

        if(copy == 0){
            icalerror_set_errno(ICAL_NEWFAILED_ERROR);
            continue;
        }

        v = strchr(copy,'=');


        if(v == 0){
            continue;
        }

        *v = 0;

        v++;

        APPENDS(", ");
        APPENDC('\'');
        APPENDS(copy);
        APPENDC('\'');
        APPENDS(sep);
        APPENDC('\'');
        APPENDS(v);        
        APPENDC('\'');
        
    }


    APPENDC('}');

    icalmemory_add_tmp_buffer(buf);
    return buf;

}

#include "fcntl.h"
int icallangbind_string_to_open_flag(const char* str)
{
    if (strcmp(str,"r") == 0) {return O_RDONLY;}
    else if (strcmp(str,"r+") == 0) {return O_RDWR;}
    else if (strcmp(str,"w") == 0) {return O_WRONLY;}
    else if (strcmp(str,"w+") == 0) {return O_RDWR|O_CREAT;}
    else if (strcmp(str,"a") == 0) {return O_WRONLY|O_APPEND;}
    else return -1;
}


const char* icallangbind_quote_as_ical(const char* str)
{
    size_t buf_size = 2 * strlen(str);

    /* assume every char could be quoted */
    char* buf = icalmemory_new_buffer(buf_size);
    int result;

    result = icalvalue_encode_ical_string(str, buf, buf_size);

    icalmemory_add_tmp_buffer(buf);

    return buf;
}
