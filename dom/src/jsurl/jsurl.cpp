/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Code liberally stolen from mkmocha.h */
#if defined(XP_MAC) && defined(NECKO)
#include "nsJSProtocolHandler.cpp"
#else

#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#ifndef NECKO
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#endif

#include <stddef.h>
#include <memory.h>
#include <time.h>
#include "net.h"
#include "jsurl.h"
#include "nsIURL.h"
#ifndef NECKO
#include "nsIConnectionInfo.h"
#endif
#include "nsIStreamListener.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptContext.h"
#include "nsString.h"

#ifndef NECKO    // out for now, until we can revive the javascript: protocol

#define MK_OUT_OF_MEMORY                -207
#define MK_MALFORMED_URL_ERROR          -209

typedef struct {
  int32             ref_count;
  ActiveEntry     * active_entry;
  PRPackedBool      is_valid;
  PRPackedBool      eval_what;
  PRPackedBool      single_shot;
  PRPackedBool      polling;
  char            * str;
  size_t            len;
  MWContext       * context;
  int               status;
  char            * base_href;
  NET_StreamClass * stream;
} JSConData;

#define HOLD_CON_DATA(con_data) ((con_data)->ref_count++)
#define DROP_CON_DATA(con_data) {                                             \
    if (--(con_data)->ref_count == 0)                                         \
      free_con_data(con_data);                                              \
}

static void
free_con_data(JSConData * con_data)
{
  PR_FREEIF(con_data->str);
  PR_FREEIF(con_data->base_href);
  PR_Free(con_data);
}

/* The following two routines were stolen from netlib */
PRIVATE void
plus_to_space(char *str)
{
    for (; *str != '\0'; str++)
		if (*str == '+')
			*str = ' ';
}


#define HEX_ESCAPE '%'
/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

PRIVATE int
unescape (char * str)
{
    register char *src = str;
    register char *dst = str;

    while(*src)
        if (*src != HEX_ESCAPE)
		  {
        	*dst++ = *src++;
		  }
        else 	
		  {
        	src++; /* walk over escape */
        	if (*src)
              {
            	*dst = UNHEX(*src) << 4;
            	src++;
              }
        	if (*src)
              {
            	*dst = (*dst + UNHEX(*src));
            	src++;
              }
        	dst++;
          }

    *dst = 0;

    return (int)(dst - str);

}

NS_DEFINE_IID(kIConnectionInfoIID,        NS_ICONNECTIONINFO_IID);
NS_DEFINE_IID(kIScriptContextOwnerIID,    NS_ISCRIPTCONTEXTOWNER_IID);

PRIVATE int32
evaluate_script(URL_Struct* urls, const char *what, JSConData *con_data)
{
  nsISupports *supports;
  nsIConnectionInfo *con_info;
  int32 result = 0;
  
  supports = (nsISupports *)urls->fe_data;
  if (supports && 
      (NS_OK == supports->QueryInterface(kIConnectionInfoIID, (void**)&con_info))) {

    nsIURI *url;
    nsISupports *viewer = nsnull;
    nsIScriptContextOwner *context_owner;

    // Get the container (which hopefully supports nsIScriptContextOwner) 
    // from the nsIURI...
    con_info->GetURL(&url);
    NS_RELEASE(con_info);

    if (nsnull != url) {
      (void)url->GetContainer(&viewer);
      NS_RELEASE(url);
    }
    // Now see if the container supports nsIScriptContextOwner...
    if (viewer &&
        (NS_OK == viewer->QueryInterface(kIScriptContextOwnerIID, (void **)&context_owner))) {
      nsIScriptContext *script_context;
      
      if (context_owner) {
        context_owner->GetScriptContext(&script_context);
        if (script_context) {
          nsAutoString ret;
          PRBool isUndefined;

          if (NS_SUCCEEDED(script_context->EvaluateString(nsString(what),
                                             nsnull, 0,
                                             ret,
                                             &isUndefined))) {
            JSContext *cx = (JSContext *)script_context->GetNativeContext();
            // Find out if it can be converted into a string
            if (!isUndefined) {
              con_data->len = ret.Length();
              con_data->str = (char *)PR_MALLOC(con_data->len + 1);
              ret.ToCString(con_data->str, con_data->len);
            }
            else {
              con_data->str = nsnull;
            }
            con_data->status = MK_DATA_LOADED;
          }
          else {
            con_data->status = MK_MALFORMED_URL_ERROR;
            result = -1;
          }
          con_data->is_valid = TRUE;
          
          NS_RELEASE(script_context);
        }
        else {
          result = -1;
        }
        NS_RELEASE(context_owner);
      }
      else {
        result = -1;
      }
      
      NS_RELEASE(viewer);
    }
    else {
      NS_IF_RELEASE(viewer);
      result = -1;
    }
  }
  else {
    result = -1;
  }
  
  return result;
}

/* forward decl */
PRIVATE int32 net_ProcessMocha(ActiveEntry * ae);

/*
 * Handle both 'mocha:<stuff>' urls and the mocha type-in widget
 */
MODULE_PRIVATE int32
net_MochaLoad(ActiveEntry *ae);
MODULE_PRIVATE int32
net_MochaLoad(ActiveEntry *ae)
{
  MWContext *context;
  URL_Struct *url_struct;
  char *what;
  Bool eval_what, single_shot;
  JSConData * con_data;
  int32 result = 0;
  
  context = ae->window_id;
  url_struct = ae->URL_s;
  what = PL_strchr(url_struct->address, ':');
  PR_ASSERT(what);
  // Get rid of : and possibly some leading /'s
  what++;
  while (*what == '/') {
    what++;
  }
  eval_what = FALSE;
  single_shot = (*what != '?');

  if (single_shot) {
    /* Don't use an existing Mocha output window's stream. */
    if (*what == '\0') {
      /* Generate two grid cells, one for output and one for input. */
      what = PR_smprintf("<frameset rows=\"75%%,25%%\">\n"
                         "<frame name=MochaOutput src=about:blank>\n"
                         "<frame name=MochaInput src=%.*s#input>\n"
                         "</frameset>",
                         what - url_struct->address,
                         url_struct->address);
    } else if (PL_strcmp(what, "#input") == 0) {
      /* The input cell contains a form with one magic isindex field. */
      what = PR_smprintf("<b>%.*s typein</b>\n"
                         "<form action=%.*s target=MochaOutput"
                         " onsubmit='this.isindex.focus()'>\n"
                         "<input name=isindex size=60>\n"
                         "</form>",
                         what - url_struct->address - 1,
                         url_struct->address,
                         what - url_struct->address,
                         url_struct->address);
      url_struct->internal_url = TRUE;
    } else {
      eval_what = TRUE;
    }
  } else {
    /* Use the Mocha output window if it exists. */
    url_struct->auto_scroll = 1000;

    /* Skip the leading question-mark and clean up the string. */
    what++;
    static char *isindex = "isindex=";
    PRInt32 ilen = PL_strlen(isindex);
    if (PL_strncmp(what, isindex, ilen) == 0) {
      what += ilen;
    }
    plus_to_space(what);
    unescape(what);
    eval_what = TRUE;
  }

  /* something got hosed.  bail */
  if (!what) {
    ae->status = MK_OUT_OF_MEMORY;
    return -1;
  }

  /* make space for the connection data */
  con_data = PR_NEWZAP(JSConData);
  if (!con_data) {
    ae->status = MK_OUT_OF_MEMORY;
    return -1;
  }

  /* remember for next time */
  con_data->ref_count = 1;
  con_data->active_entry = ae;
  ae->con_data = con_data;

  /* set up some state so we remember where we are */
  con_data->eval_what = eval_what;
  con_data->single_shot = single_shot;
  con_data->context = context;

  /* fake out netlib so we don't select on the socket id */
  ae->socket = NULL;
  ae->local_file = TRUE;

  if (eval_what) {
    result = evaluate_script(url_struct, what, con_data);
  }
  else {
    /* allocated above, don't need to free */
    con_data->str = what;
    con_data->len = PL_strlen(what);
    con_data->is_valid = TRUE;
  }

  if (-1 == result) {
    DROP_CON_DATA(con_data);
    return -1;
  }
  else {
    /* ya'll come back now, ya'hear? */
    return net_ProcessMocha(ae);
  }
}


static char *result_prefix = "<HTML><HEAD><TITLE>JavaScript typein result</TITLE></HEAD><BODY><PLAINTEXT>";

static char *result_suffix = "</PLAINTEXT></BODY></HTML>";

PRIVATE int32
net_ProcessMocha(ActiveEntry * ae)
{
  NET_StreamClass *stream = NULL;
  JSConData * con_data = (JSConData *) ae->con_data;
  MWContext * context;
  int status = 0;

    /* if we haven't gotten our data yet just return and wait */
  if (!con_data || !con_data->is_valid) {
    return 0;
  }

  context = ae->window_id;
  stream = con_data->stream;

  /* see if there were any problems */
  if (con_data->status < 0 || con_data->str == NULL) {
    ae->status = con_data->status;
    goto done;
  }

  if (!stream) {
    ae->URL_s->content_type = PL_strdup(TEXT_HTML);
    ae->format_out = CLEAR_CACHE_BIT(ae->format_out);

    stream = NET_StreamBuilder(ae->format_out, ae->URL_s, ae->window_id);
    if (!stream) {
      ae->status = MK_UNABLE_TO_CONVERT;
      goto done;
    }
  }

  /*
     * If the stream we just created isn't ready for writing just
     *   hold onto the stream and try again later
     */
  if (!stream->is_write_ready(stream)) {
    con_data->stream = stream;
    return 0;
  }

  if (con_data->eval_what) {
    status = (*stream->put_block)(stream, 
                                  result_prefix, PL_strlen(result_prefix));
  }

  if ((status >= 0) && con_data->str) {
    status = (*stream->put_block)(stream, con_data->str, con_data->len);
    if ((status >= 0) && con_data->eval_what) {
      status = (*stream->put_block)(stream, 
                                    result_suffix, PL_strlen(result_suffix));
    }
  }

  if (status >= 0) {
    (*stream->complete)(stream);
  }
  else {
    (*stream->abort)(stream, status);
  }

  PR_Free(stream);

  ae->status = MK_DATA_LOADED;

done:
    ae->con_data = NULL;
    con_data->active_entry = NULL;
    DROP_CON_DATA(con_data);
    return -1;
}

PRIVATE int32
net_InterruptMocha(ActiveEntry *ae)
{
    JSConData * con_data = (JSConData *) ae->con_data;

    if (!con_data)
      return MK_INTERRUPTED;

    /* ae is about to go away, break its connection with con_data */
    ae->con_data = NULL;
    con_data->active_entry = NULL;
    
	DROP_CON_DATA(con_data);

    return ae->status = MK_INTERRUPTED;
}

PRIVATE void
net_CleanupMocha(void)
{
    /* nothing so far needs freeing */
    return;
}

#endif // !NECKO

PR_IMPLEMENT(void)
NET_InitJavaScriptProtocol(void)
{
#ifdef NECKO
  NS_WARNING("Brendan said he would implement the javascript: protocol.");
#else
        static NET_ProtoImpl mocha_proto_impl;

        mocha_proto_impl.init = net_MochaLoad;
        mocha_proto_impl.process = net_ProcessMocha;
        mocha_proto_impl.interrupt = net_InterruptMocha;
        mocha_proto_impl.cleanup = net_CleanupMocha;

        NET_RegisterProtocolImplementation(&mocha_proto_impl, MOCHA_TYPE_URL);
#endif
}

#endif /* XP_MAC && NECKO */

