
/* this set of functions parses an incoming stream of HTML and tries to find
 * any PICS label within it.  It doesn't do anything accept parse the pics label 
 * and finish
 */
#include "mkutils.h"
#include "mkstream.h"
#include "mkgeturl.h"
#include "pa_parse.h"
#include "xp.h"
#include "xpgetstr.h"
#include "layout.h"
#include "pics.h"

typedef enum StatesEnum {
    IN_CONTENT,
    IN_SCRIPT,
    ABOUT_TO_BEGIN_TAG,
    IN_BEGIN_TAG,
    IN_TAG,
    BEGIN_ATTRIBUTE_VALUE,
    IN_QUOTED_ATTRIBUTE_VALUE,
    IN_BROKEN_QUOTED_ATTRIBUTE_VALUE,
    IN_UNQUOTED_ATTRIBUTE_VALUE,
    IN_COMMENT,
    IN_AMPERSAND_THINGY
} StatesEnum;

#define MAXTAGLEN 15

typedef struct _DataObject {
    MWContext *context;
    URL_Struct *URL_s;
    StatesEnum state;
    char tag[MAXTAGLEN+1];
    uint tag_index;
    int  tag_type;
    XP_Bool in_broken_html;
    char *tag_data;   /* the contents of the current tag */
} DataObject;

extern int MK_CVCOLOR_SOURCE_OF; 

PRIVATE void net_BeginPICSLabelFinderTag (DataObject *obj)
{

    if(obj->tag_data)
	   *obj->tag_data = '\0';  /* empty tag_data */

	if (obj->tag_type == P_SCRIPT)
	  {
		obj->tag_type = P_UNKNOWN;
	  }
	obj->state = ABOUT_TO_BEGIN_TAG;
	return ;
}

PRIVATE void net_EndPICSLabelFinderTag (DataObject *obj)
{
	if(obj->in_broken_html)
	  {
		obj->in_broken_html = FALSE;
	  }

	if (obj->tag_type == P_SCRIPT)
	  {
		obj->state = IN_SCRIPT;
	  }
	else
	  {
		obj->state = IN_CONTENT;
	  }

    /* check tag_data for a META tag */
	if(obj->tag_data && !strncasecomp(obj->tag_data, "META", 4))
	{
        PA_Tag tmp_tag;
		char *name;
        
        tmp_tag.type = P_META;
        tmp_tag.data = (void*)obj->tag_data;
        tmp_tag.data_len = XP_STRLEN((char*)tmp_tag.data);

        name = (char *)lo_FetchParamValue(obj->context, &tmp_tag, PARAM_HTTP_EQUIV);

#define PICS_HEADER "PICS-Label"
        if(name && !strcasecomp(name, PICS_HEADER))
        {
    		char *label = (char *)lo_FetchParamValue(obj->context, &tmp_tag, PARAM_CONTENT);

            if(label)
            {
                PICS_RatingsStruct * rs = PICS_ParsePICSLable(label);
                FREE(label);
                PICS_CompareToUserSettings(rs, obj->URL_s->address);
                PICS_FreeRatingsStruct(rs); /* handles NULL */
            }
        }

        FREEIF(name);
	}

	return;
}

PRIVATE int net_PICSLabelFinderWrite (NET_StreamClass *stream, CONST char *s, int32 l)
{
	int32 i;
	CONST char *cp;
	DataObject *obj = (DataObject *)stream->data_object;
    char  tiny_buf[8];

	for(i = 0, cp = s; i < l; i++, cp++)
	  {
	    switch(obj->state)
	      {
		    case IN_CONTENT:
			    /* do nothing until you find a '<' "<!--" or '&' */
				if(*cp == '<')
				  {
					/* XXX we can miss a comment spanning a block boundary */
					if(i+4 <= l && !XP_STRNCMP(cp, "<!--", 4))
					  {
						obj->state = IN_COMMENT;
					  }
					else
					  {
						net_BeginPICSLabelFinderTag(obj);
					  }
				  }
				else if(*cp == '&')
				  {
					obj->state = IN_AMPERSAND_THINGY;
				  }
			    break;
			case IN_SCRIPT:
				/* do nothing until you find '</SCRIPT>' */
				if(*cp == '<')
				  {
					/* XXX we can miss a </SCRIPT> spanning a block boundary */
					if(i+8 <= l && !XP_STRNCASECMP(cp, "</SCRIPT", 8))
					  {
						net_BeginPICSLabelFinderTag(obj);
					  }
				  }
				break;
		    case ABOUT_TO_BEGIN_TAG:
				/* we have seen the first '<'
				 * once we see a non-whitespace character
				 * we will be in the tag identifier
				 */
				if(*cp == '>')
				  {
					net_EndPICSLabelFinderTag(obj);
				  }
				else if(!XP_IS_SPACE(*cp))
				  {
                    /* capture all tag data */
				    tiny_buf[0] = *cp;
				    tiny_buf[1] = '\0';
				    StrAllocCat(obj->tag_data, tiny_buf);

					obj->state = IN_BEGIN_TAG;
					obj->tag_index = 0;
					obj->tag[obj->tag_index++] = *cp;

				  }
			    break;
		    case IN_BEGIN_TAG:
				/* go to the IN_TAG state when we see
				 * the first whitespace
				 */

				/* capture all tag data */
				tiny_buf[0] = *cp;
				tiny_buf[1] = '\0';
				StrAllocCat(obj->tag_data, tiny_buf);

				if(XP_IS_SPACE(*cp))
				  {
					obj->state = IN_TAG;
					obj->tag[obj->tag_index] = '\0';
					obj->tag_type = pa_tokenize_tag(obj->tag);
				  }
				else if(*cp == '>')
				  {
					net_EndPICSLabelFinderTag(obj);
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					if(!obj->in_broken_html)
					  {
						obj->in_broken_html = TRUE;
					  }
				  }
				else
				  {
					if (obj->tag_index < MAXTAGLEN)
						obj->tag[obj->tag_index++] = *cp;
				  }
			    break;
		    case IN_TAG:
				/* capture all tag data */
				tiny_buf[0] = *cp;
				tiny_buf[1] = '\0';
				StrAllocCat(obj->tag_data, tiny_buf);

			    /* do nothing until you find a opening '=' or end '>' */
				if(*cp == '=')
				  {
					obj->state = BEGIN_ATTRIBUTE_VALUE;
				  }
				else if(*cp == '>')
				  {
					net_EndPICSLabelFinderTag(obj);
				  }
			    break;
		    case BEGIN_ATTRIBUTE_VALUE:
				/* capture all tag data */
				tiny_buf[0] = *cp;
				tiny_buf[1] = '\0';
				StrAllocCat(obj->tag_data, tiny_buf);

				/* when we reach the first non-whitespace
				 * we will enter the UNQUOTED or the QUOTED
				 * ATTRIBUTE state
				 */
				if(!XP_IS_SPACE(*cp))
				  {
					if(*cp == '"')
                    {
		    			obj->state = IN_QUOTED_ATTRIBUTE_VALUE;
                        /* no need to jump to the quoted attr handler
                         * since this char can't be a dangerous char
                         */
                    }
					else
                    {
		    			obj->state = IN_UNQUOTED_ATTRIBUTE_VALUE;
                        /* need to jump to the unquoted attr handler
                         * since this char can be a dangerous character
                         */
                        goto unquoted_attribute_jump_point;
                    }
				  }
				else if(*cp == '>')
				  {
					net_EndPICSLabelFinderTag(obj);
				  }
			    break;
		    case IN_UNQUOTED_ATTRIBUTE_VALUE:
				/* capture all tag data */
				tiny_buf[0] = *cp;
				tiny_buf[1] = '\0';
				StrAllocCat(obj->tag_data, tiny_buf);

unquoted_attribute_jump_point:
			    /* do nothing until you find a whitespace */
				if(XP_IS_SPACE(*cp))
				  {
					obj->state = IN_TAG;
				  }
				else if(*cp == '>')
				  {
					net_EndPICSLabelFinderTag(obj);
				  }
			    break;
		    case IN_QUOTED_ATTRIBUTE_VALUE:
				/* capture all tag data */
				tiny_buf[0] = *cp;
				tiny_buf[1] = '\0';
				StrAllocCat(obj->tag_data, tiny_buf);

			    /* do nothing until you find a closing '"' */
				if(*cp == '\"')
				  {
					if(obj->in_broken_html)
					  {
						obj->in_broken_html = FALSE;
					  }
					obj->state = IN_TAG;
				  }
				else if(*cp == '>')
				  {
					/* probably a broken attribute value */
					if(!obj->in_broken_html)
					  {
						obj->in_broken_html = TRUE;
					  }
				  }
			    break;
			case IN_COMMENT:
			    /* do nothing until you find a closing '-->' */
				if(!XP_STRNCMP(cp, "-->", 3))
				  {
					cp += 2;
					i += 2;
					obj->state = IN_CONTENT;
				  }
			    break;
			case IN_AMPERSAND_THINGY:
			    /* do nothing until you find a ';' or space */
				if(*cp == ';' || XP_IS_SPACE(*cp))
				  {
					XP_SPRINTF(tiny_buf, "%c", *cp);
					obj->state = IN_CONTENT;
				  }
			    break;
		    default:
			    XP_ASSERT(0);
			    break;
		  }

	  }

	return(0);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_PICSLabelFinderWriteReady (NET_StreamClass *stream)
{
	return(MAX_WRITE_READY);
}


PRIVATE void net_PICSLabelFinderComplete (NET_StreamClass *stream)
{
}

PRIVATE void net_PICSLabelFinderAbort (NET_StreamClass *stream, int status)
{
}

PUBLIC NET_StreamClass *
net_PICSLabelFinderStream (int         format_out,
                     void       *data_obj,
                     URL_Struct *URL_s,
                     MWContext  *window_id)
{
    DataObject* obj;
	NET_StreamClass *new_stream;
	Bool is_html_stream = FALSE;

    TRACEMSG(("Setting up PICSLabelFinder stream. Have URL: %s\n", URL_s->address));

    new_stream = XP_NEW_ZAP(NET_StreamClass);

    if(!new_stream)
        return NULL;

    obj = XP_NEW(DataObject);

    if (obj == NULL)
	{
        FREE(new_stream);
        return(NULL);
	}

	XP_MEMSET(obj, 0, sizeof(DataObject));

	obj->state = IN_CONTENT;

	obj->tag_type = P_UNKNOWN;

    obj->context = window_id;
    obj->URL_s = URL_s;

    new_stream->name           = "PICSLabelFinder";
    new_stream->complete       = (MKStreamCompleteFunc) net_PICSLabelFinderComplete;
    new_stream->abort          = (MKStreamAbortFunc) net_PICSLabelFinderAbort;
    new_stream->put_block      = (MKStreamWriteFunc) net_PICSLabelFinderWrite;
    new_stream->is_write_ready = (MKStreamWriteReadyFunc)
													net_PICSLabelFinderWriteReady;
    new_stream->data_object    = (void *) obj;  /* document info object */
    new_stream->window_id      = window_id;

    /* don't cache this URL, since the content type is wrong */
    URL_s->dont_cache = TRUE;

    return new_stream;
}
