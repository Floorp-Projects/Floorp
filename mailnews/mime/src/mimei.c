/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "mimerosetta.h"

#ifdef MOZ_SECURITY
#include HG01966
#endif

#include "modmime.h"
#include "mimeobj.h"	/*  MimeObject (abstract)							*/
#include "mimecont.h"	/*   |--- MimeContainer (abstract)					*/
#include "mimemult.h"	/*   |     |--- MimeMultipart (abstract)			*/
#include "mimemmix.h"	/*   |     |     |--- MimeMultipartMixed			*/
#include "mimemdig.h"	/*   |     |     |--- MimeMultipartDigest			*/
#include "mimempar.h"	/*   |     |     |--- MimeMultipartParallel			*/
#include "mimemalt.h"	/*   |     |     |--- MimeMultipartAlternative		*/
#include "mimemrel.h"	/*   |     |     |--- MimeMultipartRelated			*/
#include "mimemapl.h"	/*   |     |     |--- MimeMultipartAppleDouble		*/
#include "mimesun.h"	/*   |     |     |--- MimeSunAttachment				*/
#include "mimemsig.h"	/*   |     |     |--- MimeMultipartSigned (abstract)*/

#ifdef MOZ_SECURITY
#include HG01921
#include HG01944
#include HG01999
#endif /* MOZ_SECURITY */

#include "mimemsg.h"	/*   |     |--- MimeMessage							*/
#include "mimeunty.h"	/*   |     |--- MimeUntypedText						*/
#include "mimeleaf.h"	/*   |--- MimeLeaf (abstract)						*/
#include "mimetext.h"	/*   |     |--- MimeInlineText (abstract)			*/
#include "mimetpla.h"	/*   |     |     |--- MimeInlineTextPlain			*/
#include "mimethtm.h"	/*   |     |     |--- MimeInlineTextHTML			*/
#include "mimetric.h"	/*   |     |     |--- MimeInlineTextRichtext		*/
#include "mimetenr.h"	/*   |     |     |     |--- MimeInlineTextEnriched	*/
/* SUPPORTED VIA PLUGIN    |     |     |--------- MimeInlineTextCalendar  */

#ifdef RICHIE_VCARD
#include "mimevcrd.h" /*   |     |     |--------- MimeInlineTextVCard		*/
#endif

#include "prefapi.h"
#include "mimeiimg.h"	/*   |     |--- MimeInlineImage						*/
#include "mimeeobj.h"	/*   |     |--- MimeExternalObject					*/
#include "mimeebod.h"	/*   |--- MimeExternalBody							*/
#include "prmem.h"
#include "prenv.h"
#include "plstr.h"
#include "prlink.h"
#include "mimecth.h"

/* ==========================================================================
   Allocation and destruction
   ==========================================================================
 */
static int mime_classinit(MimeObjectClass *class);

/* 
 * These are the necessary defines/variables for doing
 * content type handlers in external plugins.
 */
typedef struct {
  char        *content_type;
  PRBool      force_inline_display;
  char        *file_name;
  PRLibrary   *ct_handler;
} cthandler_struct;

cthandler_struct    *cthandler_list = NULL;
PRInt32             plugin_count = -1;

/* 
 * This will find the directory for content type handler plugins.
 */
PRBool
find_plugin_directory(char *path, PRInt32 size)
{
#ifdef XP_PC
  char  *ptr;

  if (!GetModuleFileName(NULL, path, size))
    return PR_FALSE;
  ptr = PL_strrchr(path, '\\');
  if (ptr)
    *ptr = '\0';
  PL_strcat(path, "\\");
  PL_strcat(path, MIME_PLUGIN_DIR);
  return PR_TRUE;
#else
  char  *tmpPath;
  tmpPath = PR_GetEnv("MOZILLA_FIVE_HOME"); 
  if (tmpPath)
  {
    PL_strcpy(path, tmpPath);
    PR_FREEIF(tmpPath);
  }
  else
    PL_strcpy(path, ".");
  

  PL_strcat(path, "/components");
  return PR_TRUE;
#endif
}

PRBool
create_file_name(const char *path, const char *name, char *fullName)
{
  if ((!path) || (!name))
    return PR_FALSE;

#ifdef XP_PC
  PL_strcpy(fullName, path);
  PL_strcat(fullName, "\\");
  PL_strcat(fullName, name);
  return PR_TRUE;
#else
  PL_strcpy(fullName, path);
  PL_strcat(fullName, "/");
  PL_strcat(fullName, name);
  return PR_TRUE;
#endif
}

/* 
 * This will locate the number of content type handler plugins.
 */
PRInt32 
get_plugin_count(void)
{
  PRDirEntry        *dirEntry;
  PRInt32           count = 0;
  PRDir             *dir;
  char              path[1024] = "";

  if (!find_plugin_directory(path, sizeof(path)))
    return 0;

  dir = PR_OpenDir(path);
  if (!dir)
    return count;

  do
  {
    dirEntry = PR_ReadDir(dir, PR_SKIP_BOTH | PR_SKIP_HIDDEN);
    if (!dirEntry)
      break;

    if (PL_strncasecmp(MIME_PLUGIN_PREFIX, dirEntry->name, PL_strlen(MIME_PLUGIN_PREFIX)) == 0)
      ++count;
  } while (dirEntry != NULL);

  PR_CloseDir(dir);
  return count;
}

char *
get_content_type(cthandler_struct *ct)
{
  mime_get_ct_fn_type     getct_fn;

  if (!ct)
    return NULL;

  getct_fn = (mime_get_ct_fn_type) PR_FindSymbol(ct->ct_handler, "MIME_GetContentType"); 
  if (!getct_fn)
    return NULL;

  return ( (getct_fn) () );
}

MimeObjectClass * 
create_content_type_handler_class(cthandler_struct *ct)
{
  contentTypeHandlerInitStruct    ctHandlerInfo;
  mime_create_class_fn_type       class_fn;
  MimeObjectClass                 *retClass = NULL;

  if (!ct)
    return NULL;

  class_fn = (mime_create_class_fn_type) PR_FindSymbol(ct->ct_handler, "MIME_CreateContentTypeHandlerClass"); 
  if (class_fn)
  {
    retClass = (class_fn)(ct->content_type, &ctHandlerInfo);
    ct->force_inline_display = ctHandlerInfo.force_inline_display;
  }

  return retClass;
}

/*
 * We need to initialize a table of external modules and
 * the content type that it will process. What we will do is check
 * the directory "mimeplugins" under the location where this DLL is
 * running. Any DLL in that directory will have the naming convention:
 * 
 * mimect-mycontenttype.dll - where mycontenttype is the content type being
 *                            processed.
 *
 * This DLL will have specifically defined entry points to be used by
 * libmime for processing the data stream.
 */
PRInt32 
do_plugin_discovery(void)
{
  PRDirEntry        *dirEntry;
  PRInt32           count = 0;
  PRDir             *dir;
  char              path[1024];
  char              full_name[1024];

  if (!find_plugin_directory(path, sizeof(path)))
    return 0;

  count = get_plugin_count();
  if (count <= 0)
    return 0;

  cthandler_list = PR_MALLOC(count * sizeof(cthandler_struct));
  if (!cthandler_list)
    return 0;
  XP_MEMSET(cthandler_list, 0, (count * sizeof(cthandler_struct)));

  dir = PR_OpenDir(path);
  if (!dir)
    return 0;

  count = 0;
  do
  {
    dirEntry = PR_ReadDir(dir, PR_SKIP_BOTH | PR_SKIP_HIDDEN);
    if (!dirEntry)
      break;

    if (PL_strncasecmp(MIME_PLUGIN_PREFIX, dirEntry->name, PL_strlen(MIME_PLUGIN_PREFIX)) == 0)
    {
      if (!create_file_name(path, dirEntry->name, full_name))
        continue;
      
      cthandler_list[count].ct_handler = PR_LoadLibrary(full_name);
      if (!cthandler_list[count].ct_handler)
        continue;

      cthandler_list[count].file_name = PL_strdup(full_name);
      if (!cthandler_list[count].file_name)
      {
        PR_UnloadLibrary(cthandler_list[count].ct_handler);
        continue;
      }

      cthandler_list[count].content_type = PL_strdup(get_content_type(&(cthandler_list[count])));
      if (!cthandler_list[count].content_type)
      {
        PR_UnloadLibrary(cthandler_list[count].ct_handler);
        PR_FREEIF(cthandler_list[count].file_name);
        continue;
      }

      ++count;
    }
  } while (dirEntry != NULL);

  PR_CloseDir(dir);
  return count;
}

/* 
 * This routine will find all content type handler for a specifc content
 * type (if it exists)
 */
MimeObjectClass *
mime_locate_external_content_handler(const char *content_type)
{
  PRInt32           i;

  if (plugin_count < 0)
    plugin_count = do_plugin_discovery();

  for (i=0; i<plugin_count; i++)
  {
    if (PL_strcasecmp(content_type, cthandler_list[i].content_type) == 0)
      return( create_content_type_handler_class((&cthandler_list[i])) );
  }

  return NULL;
}

/* 
 * This routine will find all content type handler for a specifc content
 * type (if it exists)
 */
PRBool
force_inline_display(const char *content_type)
{
  PRInt32           i;

  for (i=0; i<plugin_count; i++)
  {
    if (PL_strcasecmp(content_type, cthandler_list[i].content_type) == 0)
      return( cthandler_list[i].force_inline_display );
  }

  return PR_FALSE;
}


/* This is necessary to expose the MimeObject method outside of this DLL */
int
MIME_MimeObject_write(MimeObject *obj, char *output, PRInt32 length, PRBool user_visible_p)
{
  return MimeObject_write(obj, output, length, user_visible_p);
}

MimeObject *
mime_new (MimeObjectClass *class, MimeHeaders *hdrs,
		  const char *override_content_type)
{
  int size = class->instance_size;
  MimeObject *object;
  int status;

  /* Some assertions to verify that this isn't random junk memory... */
  PR_ASSERT(class->class_name && PL_strlen(class->class_name) > 0);
  PR_ASSERT(size > 0 && size < 1000);

  if (!class->class_initialized)
	{
	  status = mime_classinit(class);
	  if (status < 0) return 0;
	}

  PR_ASSERT(class->initialize && class->finalize);

  if (hdrs)
	{
	  hdrs = MimeHeaders_copy (hdrs);
	  if (!hdrs) return 0;
	}

  object = (MimeObject *) PR_MALLOC(size);
  if (!object) return 0;

  memset(object, 0, size);
  object->class = class;
  object->headers = hdrs;
  object->showAttachmentIcon = PR_FALSE; /* initialize ricardob's new member. */

  if (override_content_type && *override_content_type)
	object->content_type = PL_strdup(override_content_type);

  status = class->initialize(object);
  if (status < 0)
	{
	  class->finalize(object);
	  PR_Free(object);
	  return 0;
	}

  return object;
}

void
mime_free (MimeObject *object)
{
# ifdef DEBUG__
  int i, size = object->class->instance_size;
  PRUint32 *array = (PRUint32*) object;
# endif /* DEBUG */

  object->class->finalize(object);

# ifdef DEBUG__
  for (i = 0; i < (size / sizeof(*array)); i++)
	array[i] = (PRUint32) 0xDEADBEEF;
# endif /* DEBUG */

  PR_Free(object);
}


MimeObjectClass *
mime_find_class (const char *content_type, MimeHeaders *hdrs,
				 MimeDisplayOptions *opts, PRBool exact_match_p)
{
  MimeObjectClass *class = 0;
  MimeObjectClass *tempClass = 0;

  /* 
  * What we do first is check for an external content handler plugin. 
  * This will actually extend the mime handling by calling a routine 
  * which will allow us to load an external content type handler
  * for specific content types. If one is not found, we will drop back
  * to the default handler.
  */
  if ((tempClass = mime_locate_external_content_handler(content_type)) != NULL)
  {
    class = (MimeObjectClass *)tempClass;
  }
  else
  {
    if (!content_type || !*content_type ||
      !PL_strcasecmp(content_type, "text"))  /* with no / in the type */
      class = (MimeObjectClass *)&mimeUntypedTextClass;
    
      /* Subtypes of text...
    */
    else if (!PL_strncasecmp(content_type,			"text/", 5))
    {
      if      (!PL_strcasecmp(content_type+5,		"html"))
        class = (MimeObjectClass *)&mimeInlineTextHTMLClass;
      else if (!PL_strcasecmp(content_type+5,		"enriched"))
        class = (MimeObjectClass *)&mimeInlineTextEnrichedClass;
      else if (!PL_strcasecmp(content_type+5,		"richtext"))
        class = (MimeObjectClass *)&mimeInlineTextRichtextClass;
      else if (!PL_strcasecmp(content_type+5,		"plain"))
        class = (MimeObjectClass *)&mimeInlineTextPlainClass;
      
#ifdef RICHIE_VCARD
      else if (!PL_strcasecmp(content_type+5,		"x-vcard"))
        class = (MimeObjectClass *)&mimeInlineTextVCardClass;
#endif
      
      else if (!exact_match_p)
        class = (MimeObjectClass *)&mimeInlineTextPlainClass;
    }
    
    /* Subtypes of multipart...
    */
    else if (!PL_strncasecmp(content_type,			"multipart/", 10))
    {
      if      (!PL_strcasecmp(content_type+10,	"alternative"))
        class = (MimeObjectClass *)&mimeMultipartAlternativeClass;
      else if (!PL_strcasecmp(content_type+10,	"related"))
        class = (MimeObjectClass *)&mimeMultipartRelatedClass;
      else if (!PL_strcasecmp(content_type+10,	"digest"))
        class = (MimeObjectClass *)&mimeMultipartDigestClass;
      else if (!PL_strcasecmp(content_type+10,	"appledouble") ||
        !PL_strcasecmp(content_type+10,	"header-set"))
        class = (MimeObjectClass *)&mimeMultipartAppleDoubleClass;
      else if (!PL_strcasecmp(content_type+10,	"parallel"))
        class = (MimeObjectClass *)&mimeMultipartParallelClass;
      else if (!PL_strcasecmp(content_type+10,	"mixed"))
        class = (MimeObjectClass *)&mimeMultipartMixedClass;
      
      else if (!PL_strcasecmp(content_type+10,	"signed"))
      {
      /* Check that the "protocol" and "micalg" parameters are ones we
        know about. */
        char *ct = (hdrs
          ? MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE,
										PR_FALSE, PR_FALSE)
                    : 0);
        char *proto = (ct
          ? MimeHeaders_get_parameter(ct, PARAM_PROTOCOL, NULL, NULL)
          : 0);
        char *micalg = (ct
          ? MimeHeaders_get_parameter(ct, PARAM_MICALG, NULL, NULL)
          : 0);
        
#ifdef MOZ_SECURITY
        HG01444
#endif
          
          PR_FREEIF(proto);
        PR_FREEIF(micalg);
        PR_FREEIF(ct);
      }
      
      if (!class && !exact_match_p)
        /* Treat all unknown multipart subtypes as "multipart/mixed" */
        class = (MimeObjectClass *)&mimeMultipartMixedClass;
    }
    
    /* Subtypes of message...
    */
    else if (!PL_strncasecmp(content_type,			"message/", 8))
    {
      if      (!PL_strcasecmp(content_type+8,		"rfc822") ||
        !PL_strcasecmp(content_type+8,		"news"))
        class = (MimeObjectClass *)&mimeMessageClass;
      else if (!PL_strcasecmp(content_type+8,		"external-body"))
        class = (MimeObjectClass *)&mimeExternalBodyClass;
      else if (!PL_strcasecmp(content_type+8,		"partial"))
        /* I guess these are most useful as externals, for now... */
        class = (MimeObjectClass *)&mimeExternalObjectClass;
      else if (!exact_match_p)
        /* Treat all unknown message subtypes as "text/plain" */
        class = (MimeObjectClass *)&mimeInlineTextPlainClass;
    }
    
    /* The magic image types which we are able to display internally...
    */
    else if (!PL_strcasecmp(content_type,			IMAGE_GIF)  ||
      !PL_strcasecmp(content_type,			IMAGE_JPG) ||
      !PL_strcasecmp(content_type,			IMAGE_PJPG) ||
      !PL_strcasecmp(content_type,			IMAGE_PNG) ||
      !PL_strcasecmp(content_type,			IMAGE_XBM)  ||
      !PL_strcasecmp(content_type,			IMAGE_XBM2) ||
      !PL_strcasecmp(content_type,			IMAGE_XBM3))
      class = (MimeObjectClass *)&mimeInlineImageClass;
    
#ifdef MOZ_SECURITY
    HG01555
#endif
      
    /* A few types which occur in the real world and which we would otherwise
    treat as non-text types (which would be bad) without this special-case...
    */
    else if (!PL_strcasecmp(content_type,			APPLICATION_PGP) ||
    !PL_strcasecmp(content_type,			APPLICATION_PGP2))
    class = (MimeObjectClass *)&mimeInlineTextPlainClass;
    
    else if (!PL_strcasecmp(content_type,			SUN_ATTACHMENT))
      class = (MimeObjectClass *)&mimeSunAttachmentClass;
    
      /* Everything else gets represented as a clickable link.
    */
    else if (!exact_match_p)
      class = (MimeObjectClass *)&mimeExternalObjectClass;  
  }

 if (!exact_match_p)
   PR_ASSERT(class);
  if (!class) return 0;

  /* If the `Show Attachments as Links' kludge is on, now would be the time
	 to change our mind... */
  if (opts && opts->no_inline_p)
	{
	  if (mime_subclass_p(class, (MimeObjectClass *)&mimeInlineTextClass))
		{
		  /* It's a text type.  Write it only if it's the *first* part
			 that we're writing, and then only if it has no "filename"
			 specified (the assumption here being, if it has a filename,
			 it wasn't simply typed into the text field -- it was actually
			 an attached document.) */
		  if (opts->state && opts->state->first_part_written_p)
			class = (MimeObjectClass *)&mimeExternalObjectClass;
		  else
			{
			  /* If there's a name, then write this as an attachment. */
			  char *name = (hdrs ? MimeHeaders_get_name(hdrs) : 0);
			  if (name)
				class = (MimeObjectClass *)&mimeExternalObjectClass;
			  PR_FREEIF(name);
			}

		  if (opts->state)
			opts->state->first_part_written_p = PR_TRUE;
		}
	  else if (mime_subclass_p(class,(MimeObjectClass *)&mimeContainerClass) &&
			   !mime_subclass_p(class,(MimeObjectClass *)&mimeMessageClass))
		/* Multipart subtypes are ok, except for messages; descend into
		   multiparts, and defer judgement.

		   Xlateed blobs are just like other containers (make the xlation
		   layer invisible, and treat them as simple containers.  So there's
		   no easy way to save xlated data directly to disk; it will tend
		   to always be wrapped inside a message/rfc822.  That's ok.)
		 */
		;
	  else if (opts &&
			   opts->part_to_load &&
			   mime_subclass_p(class,(MimeObjectClass *)&mimeMessageClass))
		/* Descend into messages only if we're looking for a specific sub-part.
		 */
		;
	  else
		{
		  /* Anything else, and display it as a link (and cause subsequent
			 text parts to also be displayed as links.) */
		  class = (MimeObjectClass *)&mimeExternalObjectClass;
		  if (opts->state)
			opts->state->first_part_written_p = PR_TRUE;
		}
	}

  PR_ASSERT(class);

  if (class && !class->class_initialized)
	{
	  int status = mime_classinit(class);
	  if (status < 0) return 0;
	}

  return class;
}


MimeObject *
mime_create (const char *content_type, MimeHeaders *hdrs,
			 MimeDisplayOptions *opts)
{
  /* If there is no Content-Disposition header, or if the Content-Disposition
	 is ``inline'', then we display the part inline (and let mime_find_class()
	 decide how.)

	 If there is any other Content-Disposition (either ``attachment'' or some
	 disposition that we don't recognise) then we always display the part as
	 an external link, by using MimeExternalObject to display it.

	 But Content-Disposition is ignored for all containers except `message'.
	 (including multipart/mixed, and multipart/digest.)  It's not clear if
	 this is to spec, but from a usability standpoint, I think it's necessary.
   */

  MimeObjectClass *class = 0;
  char *content_disposition = 0;
  MimeObject *obj = 0;
  char *override_content_type = 0;
  static PRBool reverse_lookup = PR_FALSE, got_lookup_pref = PR_FALSE;

  if (!got_lookup_pref)
  {
	  PREF_GetBoolPref("mailnews.autolookup_unknown_mime_types",&reverse_lookup);
	  got_lookup_pref = PR_TRUE;
  }


  /* There are some clients send out all attachments with a content-type
	 of application/octet-stream.  So, if we have an octet-stream attachment,
	 try to guess what type it really is based on the file extension.  I HATE
	 that we have to do this...

     If the preference "mailnews.autolookup_unknown_mime_types" is set to PR_TRUE,
     then we try to do this EVERY TIME when we do not have an entry for the given
	 MIME type in our table, not only when it's application/octet-stream. */
  if (hdrs && opts && opts->file_type_fn &&

	  /* ### mwelch - don't override AppleSingle */
	  (content_type ? PL_strcasecmp(content_type, APPLICATION_APPLEFILE) : PR_TRUE) &&
	  /* ## davidm Apple double shouldn't use this #$%& either. */
	  (content_type ? PL_strcasecmp(content_type, MULTIPART_APPLEDOUBLE) : PR_TRUE) &&
	  (!content_type ||
	   !PL_strcasecmp(content_type, APPLICATION_OCTET_STREAM) ||
	   !PL_strcasecmp(content_type, UNKNOWN_CONTENT_TYPE) ||
	   (reverse_lookup &&
	    !NET_cinfo_find_info_by_type((char*)content_type))))
	{
	  char *name = MimeHeaders_get_name(hdrs);
	  if (name)
		{
		  override_content_type = opts->file_type_fn (name,
													  opts->stream_closure);
		  PR_FREEIF(name);

		  if (override_content_type &&
			  !PL_strcasecmp(override_content_type, UNKNOWN_CONTENT_TYPE))
			PR_FREEIF(override_content_type);

		  if (override_content_type)
			content_type = override_content_type;
		}
	}


  class = mime_find_class(content_type, hdrs, opts, PR_FALSE);

  PR_ASSERT(class);
  if (!class) goto FAIL;

  if (opts && opts->part_to_load)
	/* Always ignore Content-Disposition when we're loading some specific
	   sub-part (which may be within some container that we wouldn't otherwise
	   descend into, if the container itself had a Content-Disposition of
	   `attachment'. */
	content_disposition = 0;

  else if (mime_subclass_p(class,(MimeObjectClass *)&mimeContainerClass) &&
		   !mime_subclass_p(class,(MimeObjectClass *)&mimeMessageClass))
	/* Ignore Content-Disposition on all containers except `message'.
	   That is, Content-Disposition is ignored for multipart/mixed objects,
	   but is obeyed for message/rfc822 objects. */
	content_disposition = 0;

  else
  {
	/* change content-Disposition for vcards to be inline so */
	/* we can see a nice html display */
#ifdef RICHIE_VCARD
    if (mime_subclass_p(class,(MimeObjectClass *)&mimeInlineTextVCardClass))
  		StrAllocCopy(content_disposition, "inline");
	  else
#endif

    /* Check to see if the plugin should override the content disposition
       to make it appear inline. One example is a vcard which has a content
       disposition of an "attachment;" */
    if (force_inline_display(content_type))
  		StrAllocCopy(content_disposition, "inline");
    else
  		content_disposition = (hdrs
							   ? MimeHeaders_get(hdrs, HEADER_CONTENT_DISPOSITION, PR_TRUE, PR_FALSE)
							   : 0);
  }

  if (!content_disposition || !PL_strcasecmp(content_disposition, "inline"))
    ;	/* Use the class we've got. */
  else
    class = (MimeObjectClass *)&mimeExternalObjectClass;

  PR_FREEIF(content_disposition);
  obj = mime_new (class, hdrs, content_type);

 FAIL:

  /* If we decided to ignore the content-type in the headers of this object
	 (see above) then make sure that our new content-type is stored in the
	 object itself.  (Or free it, if we're in an out-of-memory situation.)
   */
  if (override_content_type)
	{
	  if (obj)
		{
		  PR_FREEIF(obj->content_type);
		  obj->content_type = override_content_type;
		}
	  else
		{
		  PR_Free(override_content_type);
		}
	}

  return obj;
}



static int mime_classinit_1(MimeObjectClass *class, MimeObjectClass *target);

static int
mime_classinit(MimeObjectClass *class)
{
  int status;
  if (class->class_initialized)
	return 0;

  PR_ASSERT(class->class_initialize);
  if (!class->class_initialize)
	return -1;

  /* First initialize the superclass.
   */
  if (class->superclass && !class->superclass->class_initialized)
	{
	  status = mime_classinit(class->superclass);
	  if (status < 0) return status;
	}

  /* Now run each of the superclass-init procedures in turn,
	 parentmost-first. */
  status = mime_classinit_1(class, class);
  if (status < 0) return status;

  /* Now we're done. */
  class->class_initialized = PR_TRUE;
  return 0;
}

static int
mime_classinit_1(MimeObjectClass *class, MimeObjectClass *target)
{
  int status;
  if (class->superclass)
	{
	  status = mime_classinit_1(class->superclass, target);
	  if (status < 0) return status;
	}
  return class->class_initialize(target);
}


PRBool
mime_subclass_p(MimeObjectClass *child, MimeObjectClass *parent)
{
  if (child == parent)
	return PR_TRUE;
  else if (!child->superclass)
	return PR_FALSE;
  else
	return mime_subclass_p(child->superclass, parent);
}

PRBool
mime_typep(MimeObject *obj, MimeObjectClass *class)
{
  return mime_subclass_p(obj->class, class);
}



/* URL munging
 */


/* Returns a string describing the location of the part (like "2.5.3").
   This is not a full URL, just a part-number.
 */
char *
mime_part_address(MimeObject *obj)
{
  if (!obj->parent)
	return PL_strdup("0");
  else
	{
	  /* Find this object in its parent. */
	  PRInt32 i, j = -1;
	  char buf [20];
	  char *higher = 0;
	  MimeContainer *cont = (MimeContainer *) obj->parent;
	  PR_ASSERT(mime_typep(obj->parent,
						   (MimeObjectClass *)&mimeContainerClass));
	  for (i = 0; i < cont->nchildren; i++)
		if (cont->children[i] == obj)
		  {
			j = i+1;
			break;
		  }
	  if (j == -1)
		{
		  PR_ASSERT(0);
		  return 0;
		}

	  XP_SPRINTF(buf, "%ld", j);
	  if (obj->parent->parent)
		{
		  higher = mime_part_address(obj->parent);
		  if (!higher) return 0;  /* MK_OUT_OF_MEMORY */
		}

	  if (!higher)
		return PL_strdup(buf);
	  else
		{
		  char *s = PR_MALLOC(PL_strlen(higher) + PL_strlen(buf) + 3);
		  if (!s)
			{
			  PR_Free(higher);
			  return 0;  /* MK_OUT_OF_MEMORY */
			}
		  PL_strcpy(s, higher);
		  PL_strcat(s, ".");
		  PL_strcat(s, buf);
		  PR_Free(higher);
		  return s;
		}
	}
}


/* Returns a string describing the location of the *IMAP* part (like "2.5.3").
   This is not a full URL, just a part-number.
   This part is explicitly passed in the X-Mozilla-IMAP-Part header.
   Return value must be freed by the caller.
 */
char *
mime_imap_part_address(MimeObject *obj)
{
	char *imap_part = 0;
	if (!obj || !obj->headers)
		return 0;
	
	imap_part = MimeHeaders_get(obj->headers,
		IMAP_EXTERNAL_CONTENT_HEADER, PR_FALSE, PR_FALSE);

	return imap_part;
}

#ifdef MOZ_SECURITY
  HG08555
  HG92103
  HG08232
#endif 

/* Puts a part-number into a URL.  If append_p is true, then the part number
   is appended to any existing part-number already in that URL; otherwise,
   it replaces it.
 */
char *
mime_set_url_part(const char *url, char *part, PRBool append_p)
{
  const char *part_begin = 0;
  const char *part_end = 0;
  PRBool got_q = PR_FALSE;
  const char *s;
  char *result;

  for (s = url; *s; s++)
	{
	  if (*s == '?')
		{
		  got_q = PR_TRUE;
		  if (!PL_strncasecmp(s, "?part=", 6))
			part_begin = (s += 6);
		}
	  else if (got_q && *s == '&' && !PL_strncasecmp(s, "&part=", 6))
		part_begin = (s += 6);

	  if (part_begin)
		{
		  for (; (*s && *s != '?' && *s != '&'); s++)
			;
		  part_end = s;
		  break;
		}
	}

  result = (char *) PR_MALLOC(PL_strlen(url) + PL_strlen(part) + 10);
  if (!result) return 0;

  if (part_begin)
	{
	  if (append_p)
		{
		  XP_MEMCPY(result, url, part_end - url);
		  result [part_end - url]     = '.';
		  result [part_end - url + 1] = 0;
		}
	  else
		{
		  XP_MEMCPY(result, url, part_begin - url);
		  result [part_begin - url] = 0;
		}
	}
  else
	{
	  PL_strcpy(result, url);
	  if (got_q)
		PL_strcat(result, "&part=");
	  else
		PL_strcat(result, "?part=");
	}

  PL_strcat(result, part);

  if (part_end && *part_end)
	PL_strcat(result, part_end);

  /* Semi-broken kludge to omit a trailing "?part=0". */
  {
	int L = PL_strlen(result);
	if (L > 6 &&
		(result[L-7] == '?' || result[L-7] == '&') &&
		!PL_strcmp("part=0", result + L - 6))
	  result[L-7] = 0;
  }

  return result;
}



/* Puts an *IMAP* part-number into a URL.
   Strips off any previous *IMAP* part numbers, since they are absolute, not relative.
 */
char *
mime_set_url_imap_part(const char *url, char *imappart, char *libmimepart)
{
  char *result = 0;
  char *whereCurrent = PL_strstr(url, "/;section=");
  if (whereCurrent)
  {
	  *whereCurrent = 0;
  }
	
  result = (char *) PR_MALLOC(PL_strlen(url) + PL_strlen(imappart) + PL_strlen(libmimepart) + 17);
  if (!result) return 0;

  PL_strcpy(result, url);
  PL_strcat(result, "/;section=");
  PL_strcat(result, imappart);
  PL_strcat(result, "&part=");
  PL_strcat(result, libmimepart);
  result[PL_strlen(result)] = 0;

  if (whereCurrent)
	  *whereCurrent = '/';

  return result;
}


/* Given a part ID, looks through the MimeObject tree for a sub-part whose ID
   number matches, and returns the MimeObject (else NULL.)
   (part is not a URL -- it's of the form "1.3.5".)
 */
MimeObject *
mime_address_to_part(const char *part, MimeObject *obj)
{
  /* Note: this is an N^2 operation, but the number of parts in a message
	 shouldn't ever be large enough that this really matters... */

  PRBool match;

  if (!part || !*part)
	{
	  match = !obj->parent;
	}
  else
	{
	  char *part2 = mime_part_address(obj);
	  if (!part2) return 0;  /* MK_OUT_OF_MEMORY */
	  match = !PL_strcmp(part, part2);
	  PR_Free(part2);
	}

  if (match)
	{
	  /* These are the droids we're looking for. */
	  return obj;
	}
  else if (!mime_typep(obj, (MimeObjectClass *) &mimeContainerClass))
	{
	  /* Not a container, pull up, pull up! */
	  return 0;
	}
  else
	{
	  PRInt32 i;
	  MimeContainer *cont = (MimeContainer *) obj;
	  for (i = 0; i < cont->nchildren; i++)
		{
		  MimeObject *o2 = mime_address_to_part(part, cont->children[i]);
		  if (o2) return o2;
		}
	  return 0;
	}
}

/* Given a part ID, looks through the MimeObject tree for a sub-part whose ID
   number matches; if one is found, returns the Content-Name of that part.
   Else returns NULL.  (part is not a URL -- it's of the form "1.3.5".)
 */
char *
mime_find_content_type_of_part(const char *part, MimeObject *obj)
{
  char *result = 0;

  obj = mime_address_to_part(part, obj);
  if (!obj) return 0;

  result = (obj->headers ? MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE) : 0);

  return result;
}

/* Given a part ID, looks through the MimeObject tree for a sub-part whose ID
   number matches; if one is found, returns the Content-Name of that part.
   Else returns NULL.  (part is not a URL -- it's of the form "1.3.5".)
 */
char *
mime_find_suggested_name_of_part(const char *part, MimeObject *obj)
{
  char *result = 0;

  obj = mime_address_to_part(part, obj);
  if (!obj) return 0;

  result = (obj->headers ? MimeHeaders_get_name(obj->headers) : 0);

  /* If this part doesn't have a name, but this part is one fork of an
	 AppleDouble, and the AppleDouble itself has a name, then use that. */
  if (!result &&
	  obj->parent &&
	  obj->parent->headers &&
	  mime_typep(obj->parent,
				 (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	result = MimeHeaders_get_name(obj->parent->headers);

  /* Else, if this part is itself an AppleDouble, and one of its children
	 has a name, then use that (check data fork first, then resource.) */
  if (!result &&
	  mime_typep(obj, (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	{
	  MimeContainer *cont = (MimeContainer *) obj;
	  if (cont->nchildren > 1 &&
		  cont->children[1] &&
		  cont->children[1]->headers)
		result = MimeHeaders_get_name(cont->children[1]->headers);

	  if (!result &&
		  cont->nchildren > 0 &&
		  cont->children[0] &&
		  cont->children[0]->headers)
		result = MimeHeaders_get_name(cont->children[0]->headers);
	}

  /* Ok, now we have the suggested name, if any.
	 Now we remove any extensions that correspond to the
	 Content-Transfer-Encoding.  For example, if we see the headers

		Content-Type: text/plain
		Content-Disposition: inline; filename=foo.text.uue
		Content-Transfer-Encoding: x-uuencode

	 then we would look up (in mime.types) the file extensions which are
	 associated with the x-uuencode encoding, find that "uue" is one of
	 them, and remove that from the end of the file name, thus returning
	 "foo.text" as the name.  This is because, by the time this file ends
	 up on disk, its content-transfer-encoding will have been removed;
	 therefore, we should suggest a file name that indicates that.
   */
  if (result && obj->encoding && *obj->encoding)
	{
	  PRInt32 L = PL_strlen(result);
	  const char **exts = 0;

	  /*
		 I'd like to ask the mime.types file, "what extensions correspond
		 to obj->encoding (which happens to be "x-uuencode") but doing that
		 in a non-sphagetti way would require brain surgery.  So, since
		 currently uuencode is the only content-transfer-encoding which we
		 understand which traditionally has an extension, we just special-
		 case it here!  Icepicks in my forehead!

		 Note that it's special-cased in a similar way in libmsg/compose.c.
	   */
	  if (!PL_strcasecmp(obj->encoding, ENCODING_UUENCODE))
		{
		  static const char *uue_exts[] = { "uu", "uue", 0 };
		  exts = uue_exts;
		}

	  while (exts && *exts)
		{
		  const char *ext = *exts;
		  PRInt32 L2 = PL_strlen(ext);
		  if (L > L2 + 1 &&							/* long enough */
			  result[L - L2 - 1] == '.' &&			/* '.' in right place*/
			  !PL_strcasecmp(ext, result + (L - L2)))	/* ext matches */
			{
			  result[L - L2 - 1] = 0;		/* truncate at '.' and stop. */
			  break;
			}
		  exts++;
		}
	}

  return result;
}

#ifdef MOZ_SECURITY
HG78888
#endif

/* Parse the various "?" options off the URL and into the options struct.
 */
int
mime_parse_url_options(const char *url, MimeDisplayOptions *options)
{
  const char *q;
  MimeHeadersState default_headers = options->headers;

  if (!url || !*url) return 0;
  if (!options) return 0;

  q = PL_strrchr (url, '?');
  if (! q) return 0;
  q++;
  while (*q)
	{
	  const char *end, *value, *name_end;
	  for (end = q; *end && *end != '&'; end++)
		;
	  for (value = q; *value != '=' && value < end; value++)
		;
	  name_end = value;
	  if (value < end) value++;
	  if (name_end <= q)
		;
	  else if (!PL_strncasecmp ("headers", q, name_end - q))
		{
		  if (end > value && !PL_strncasecmp ("all", value, end - value))
			options->headers = MimeHeadersAll;
		  else if (end > value && !PL_strncasecmp ("some", value, end - value))
			options->headers = MimeHeadersSome;
		  else if (end > value && !PL_strncasecmp ("micro", value, end - value))
			options->headers = MimeHeadersMicro;
		  else if (end > value && !PL_strncasecmp ("cite", value, end - value))
			options->headers = MimeHeadersCitation;
		  else if (end > value && !PL_strncasecmp ("citation", value, end-value))
			options->headers = MimeHeadersCitation;
		  else
			options->headers = default_headers;
		}
	  else if (!PL_strncasecmp ("part", q, name_end - q))
		{
		  PR_FREEIF (options->part_to_load);
		  if (end > value)
			{
			  options->part_to_load = (char *) PR_MALLOC(end - value + 1);
			  if (!options->part_to_load)
				return MK_OUT_OF_MEMORY;
			  XP_MEMCPY(options->part_to_load, value, end-value);
			  options->part_to_load[end-value] = 0;
			}
		}
	  else if (!PL_strncasecmp ("rot13", q, name_end - q))
		{
		  if (end <= value || !PL_strncasecmp ("true", value, end - value))
			options->rot13_p = PR_TRUE;
		  else
			options->rot13_p = PR_FALSE;
		}
	  else if (!PL_strncasecmp ("inline", q, name_end - q))
		{
		  if (end <= value || !PL_strncasecmp ("true", value, end - value))
			options->no_inline_p = PR_FALSE;
		  else
			options->no_inline_p = PR_TRUE;
		}

	  q = end;
	  if (*q)
		q++;
	}


  /* Compatibility with the "?part=" syntax used in the old (Mozilla 2.0)
	 MIME parser.

	 Basically, the problem is that the old part-numbering code was totally
	 busted: here's a comparison of the old and new numberings with a pair
	 of hypothetical messages (one with a single part, and one with nested
	 containers.)
                               NEW:      OLD:  OR:
         message/rfc822
           image/jpeg           1         0     0

         message/rfc822
           multipart/mixed      1         0     0
             text/plain         1.1       1     1
             image/jpeg         1.2       2     2
             message/rfc822     1.3       -     3
               text/plain       1.3.1     3     -
             message/rfc822     1.4       -     4
               multipart/mixed  1.4.1     4     -
                 text/plain     1.4.1.1   4.1   -
                 image/jpeg     1.4.1.2   4.2   -
             text/plain         1.5       5     5

	 The "NEW" column is how the current code counts.  The "OLD" column is
	 what "?part=" references would do in 3.0b4 and earlier; you'll see that
	 you couldn't directly refer to the child message/rfc822 objects at all!
	 But that's when it got really weird, because if you turned on
	 "Attachments As Links" (or used a URL like "?inline=false&part=...")
	 then you got a totally different numbering system (seen in the "OR"
	 column.)  Gag!

	 So, the problem is, ClariNet had been using these part numbers in their
	 HTML news feeds, as a sleazy way of transmitting both complex HTML layouts
	 and images using NNTP as transport, without invoking HTTP.

	 The following clause is to provide some small amount of backward
	 compatibility.  By looking at that table, one can see that in the new
	 model, "part=0" has no meaning, and neither does "part=2" or "part=3"
	 and so on.

     "part=1" is ambiguous between the old and new way, as is any part
	 specification that has a "." in it.

	 So, the compatibility hack we do here is: if the part is "0", then map
	 that to "1".  And if the part is >= "2", then prepend "1." to it (so that
	 we map "2" to "1.2", and "3" to "1.3".)

	 This leaves the URLs compatible in the cases of:

	   = single part messages
	   = references to elements of a top-level multipart except the first

     and leaves them incompatible for:

	   = the first part of a top-level multipart
	   = all elements deeper than the outermost part

	 Life s#$%s when you don't properly think out things that end up turning
	 into de-facto standards...
   */
  if (options->part_to_load &&
	  !PL_strchr(options->part_to_load, '.'))		/* doesn't contain a dot */
	{
	  if (!PL_strcmp(options->part_to_load, "0"))		/* 0 */
		{
		  PR_Free(options->part_to_load);
		  options->part_to_load = PL_strdup("1");
		  if (!options->part_to_load)
			return MK_OUT_OF_MEMORY;
		}
	  else if (PL_strcmp(options->part_to_load, "1"))	/* not 1 */
		{
		  const char *prefix = "1.";
		  char *s = (char *) PR_MALLOC(PL_strlen(options->part_to_load) +
									  PL_strlen(prefix) + 1);
		  if (!s) return MK_OUT_OF_MEMORY;
		  PL_strcpy(s, prefix);
		  PL_strcat(s, options->part_to_load);
		  PR_Free(options->part_to_load);
		  options->part_to_load = s;
		}
	}


  return 0;
}


/* Some output-generation utility functions...
 */

int
MimeOptions_write(MimeDisplayOptions *opt, char *data, PRInt32 length,
				  PRBool user_visible_p)
{
  int status = 0;
  void* closure = 0;
  if (!opt || !opt->output_fn || !opt->state)
	return 0;

  closure = opt->output_closure;
  if (!closure) closure = opt->stream_closure;

  PR_ASSERT(opt->state->first_data_written_p);

  if (opt->state->separator_queued_p && user_visible_p)
	{
	  opt->state->separator_queued_p = PR_FALSE;
	  if (opt->state->separator_suppressed_p)
		opt->state->separator_suppressed_p = PR_FALSE;
	  else
		{
		  char sep[] = "<HR WIDTH=\"90%\" SIZE=4>";
		  int status = opt->output_fn(sep, PL_strlen(sep), closure);
		  opt->state->separator_suppressed_p = PR_FALSE;
		  if (status < 0) return status;
		}
	}
  if (user_visible_p)
	opt->state->separator_suppressed_p = PR_FALSE;

  if (length > 0)
	{
	  status = opt->output_fn(data, length, closure);
	  if (status < 0) return status;
	}

  return 0;
}

int
MimeObject_write(MimeObject *obj, char *output, PRInt32 length,
				 PRBool user_visible_p)
{
  if (!obj->output_p) return 0;

  if (!obj->options->state->first_data_written_p)
	{
	  int status = MimeObject_output_init(obj, 0);
	  if (status < 0) return status;
	  PR_ASSERT(obj->options->state->first_data_written_p);
	}

  return MimeOptions_write(obj->options, output, length, user_visible_p);
}

int
MimeObject_write_separator(MimeObject *obj)
{
  if (obj->options && obj->options->state)
	obj->options->state->separator_queued_p = PR_TRUE;
  return 0;
}

int
MimeObject_output_init(MimeObject *obj, const char *content_type)
{
  if (obj &&
	  obj->options &&
	  obj->options->state &&
	  !obj->options->state->first_data_written_p)
	{
	  int status;
	  const char *charset = 0;
	  char *name = 0, *x_mac_type = 0, *x_mac_creator = 0;

	  if (!obj->options->output_init_fn)
		{
		  obj->options->state->first_data_written_p = PR_TRUE;
		  return 0;
		}

	  if (obj->headers)
		{
		  char *ct;
		  name = MimeHeaders_get_name(obj->headers);

		  ct = MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE,
							   PR_FALSE, PR_FALSE);
		  if (ct)
			{
			  x_mac_type   = MimeHeaders_get_parameter(ct,PARAM_X_MAC_TYPE, NULL, NULL);
			  x_mac_creator= MimeHeaders_get_parameter(ct,PARAM_X_MAC_CREATOR, NULL, NULL);
			  PR_FREEIF(obj->options->default_charset);
			  obj->options->default_charset = MimeHeaders_get_parameter(ct, "charset", NULL, NULL);
			  PR_Free(ct);
			}
		}

	  if (mime_typep(obj, (MimeObjectClass *) &mimeInlineTextClass))
		charset = ((MimeInlineText *)obj)->charset;

	  if (!content_type)
		content_type = obj->content_type;
	  if (!content_type)
		content_type = TEXT_PLAIN;

	  status = obj->options->output_init_fn (content_type, charset, name,
											 x_mac_type, x_mac_creator,
											 obj->options->stream_closure);
	  PR_FREEIF(name);
	  PR_FREEIF(x_mac_type);
	  PR_FREEIF(x_mac_creator);
	  obj->options->state->first_data_written_p = PR_TRUE;
	  return status;
	}
  return 0;
}
