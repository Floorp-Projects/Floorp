/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

#include "nsCOMPtr.h"
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
#ifdef ENABLE_SMIME
#include "mimemcms.h" /*   |     |           |---MimeMultipartSignedCMS */
#endif
#include "mimecryp.h"	/*   |     |--- MimeEncrypted (abstract)			*/
#ifdef ENABLE_SMIME
#include "mimecms.h"	/*   |     |     |--- MimeEncryptedPKCS7			*/
#endif

#include "mimemsg.h"	/*   |     |--- MimeMessage							*/
#include "mimeunty.h"	/*   |     |--- MimeUntypedText						*/
#include "mimeleaf.h"	/*   |--- MimeLeaf (abstract)						*/
#include "mimetext.h"	/*   |     |--- MimeInlineText (abstract)			*/
#include "mimetpla.h"	/*   |     |     |--- MimeInlineTextPlain			*/
#include "mimetpfl.h" /*   |     |     |--- MimeInlineTextPlainFlowed         */
#include "mimethtm.h"	/*   |     |     |--- MimeInlineTextHTML			*/
#include "mimetric.h"	/*   |     |     |--- MimeInlineTextRichtext		*/
#include "mimetenr.h"	/*   |     |     |     |--- MimeInlineTextEnriched	*/
/* SUPPORTED VIA PLUGIN    |     |     |--------- MimeInlineTextCalendar  */

#include "nsIPref.h"
#include "mimeiimg.h"	/*   |     |--- MimeInlineImage						*/
#include "mimeeobj.h"	/*   |     |--- MimeExternalObject					*/
#include "mimeebod.h"	/*   |--- MimeExternalBody							*/
#include "prlog.h"
#include "prmem.h"
#include "prenv.h"
#include "plstr.h"
#include "prlink.h"
#include "prprf.h"
#include "mimecth.h"
#include "mimebuf.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "mimemoz2.h"
#include "nsIMimeContentTypeHandler.h"
#include "nsIComponentManager.h"
#include "nsVoidArray.h"
#include "nsMimeStringResources.h"
#include "nsMimeTypes.h"
#include "nsMsgUtils.h"

#define	IMAP_EXTERNAL_CONTENT_HEADER "X-Mozilla-IMAP-Part"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/* ==========================================================================
   Allocation and destruction
   ==========================================================================
 */
static int mime_classinit(MimeObjectClass *clazz);

/* 
 * These are the necessary defines/variables for doing
 * content type handlers in external plugins.
 */
typedef struct {
  char        content_type[128];
  PRBool      force_inline_display;
} cthandler_struct;

nsVoidArray         *ctHandlerList = NULL;
PRBool              foundIt = PR_FALSE; 
PRBool              force_display = PR_FALSE;

PRBool PR_CALLBACK
EnumFunction(void* aElement, void *aData)
{
  cthandler_struct    *ptr = (cthandler_struct *) aElement;
  char                *ctPtr = (char *)aData;

  if ( (!aElement) || (!aData) )
    return PR_TRUE;

  if (nsCRT::strcasecmp(ctPtr, ptr->content_type) == 0)
  {
    foundIt = PR_TRUE;
    force_display = ptr->force_inline_display;
    return PR_FALSE;
  }

  return PR_TRUE;
}

/*
 * This will return TRUE if the content_type is found in the
 * list, FALSE if it is not found. 
 */
PRBool
find_content_type_attribs(const char *content_type, 
                          PRBool     *force_inline_display)
{
  *force_inline_display = PR_FALSE;
  if (!ctHandlerList)
    return PR_FALSE;

  foundIt = PR_FALSE;
  force_display = PR_FALSE;
  ctHandlerList->EnumerateForwards(EnumFunction, (void *)content_type);
  if (foundIt)
    *force_inline_display = force_display;

  return (foundIt);
}

void
add_content_type_attribs(const char *content_type, 
                         contentTypeHandlerInitStruct  *ctHandlerInfo)
{
  cthandler_struct    *ptr = NULL;
  PRBool              force_inline_display;

  if (find_content_type_attribs(content_type, &force_inline_display))
    return;

  if ( (!content_type) || (!ctHandlerInfo) )
    return;

  if (!ctHandlerList)
    ctHandlerList = new nsVoidArray();

  if (!ctHandlerList)
    return;

  ptr = (cthandler_struct *) PR_MALLOC(sizeof(cthandler_struct));
  if (!ptr)
    return;

  PL_strncpy(ptr->content_type, content_type, sizeof(ptr->content_type));
  ptr->force_inline_display = ctHandlerInfo->force_inline_display;
  ctHandlerList->AppendElement(ptr);
}

/* 
 * This routine will find all content type handler for a specifc content
 * type (if it exists)
 */
PRBool
force_inline_display(const char *content_type)
{
  PRBool     force_inline_disp;

  find_content_type_attribs(content_type, &force_inline_disp);
  return (force_inline_disp);
}

/* 
 * This routine will find all content type handler for a specifc content
 * type (if it exists) and is defined to the nsRegistry
 */
MimeObjectClass *
mime_locate_external_content_handler(const char *content_type,   
                                     contentTypeHandlerInitStruct  *ctHandlerInfo)
{
  MimeObjectClass               *newObj = NULL;
  nsCID                         classID = {0};
  char                          lookupID[256];
  nsCOMPtr<nsIMimeContentTypeHandler>     ctHandler;
  nsresult rv = NS_OK;

  PR_snprintf(lookupID, sizeof(lookupID), "@mozilla.org/mimecth;1?type=%s", content_type);
	if (nsComponentManager::ContractIDToClassID(lookupID, &classID) != NS_OK)
    return NULL;
  
  rv  = nsComponentManager::CreateInstance(classID, (nsISupports *)nsnull, NS_GET_IID(nsIMimeContentTypeHandler), 
                                     (void **) getter_AddRefs(ctHandler));
  if (NS_FAILED(rv) || !ctHandler)
    return nsnull;
  
  rv = ctHandler->CreateContentTypeHandlerClass(content_type, ctHandlerInfo, &newObj);
  if (NS_FAILED(rv))
    return nsnull;

  add_content_type_attribs(content_type, ctHandlerInfo);
  return newObj;
}

/* This is necessary to expose the MimeObject method outside of this DLL */
int
MIME_MimeObject_write(MimeObject *obj, char *output, PRInt32 length, PRBool user_visible_p)
{
  return MimeObject_write(obj, output, length, user_visible_p);
}

MimeObject *
mime_new (MimeObjectClass *clazz, MimeHeaders *hdrs,
		  const char *override_content_type)
{
  int size = clazz->instance_size;
  MimeObject *object;
  int status;

  /* Some assertions to verify that this isn't random junk memory... */
  PR_ASSERT(clazz->class_name && nsCRT::strlen(clazz->class_name) > 0);
  PR_ASSERT(size > 0 && size < 1000);

  if (!clazz->class_initialized)
	{
	  status = mime_classinit(clazz);
	  if (status < 0) return 0;
	}

  PR_ASSERT(clazz->initialize && clazz->finalize);

  if (hdrs)
	{
    hdrs = MimeHeaders_copy (hdrs);
	  if (!hdrs) return 0;
	}

  object = (MimeObject *) PR_MALLOC(size);
  if (!object) return 0;

  memset(object, 0, size);
  object->clazz = clazz;
  object->headers = hdrs;
  object->showAttachmentIcon = PR_FALSE; /* initialize ricardob's new member. */

  if (override_content_type && *override_content_type)
	object->content_type = nsCRT::strdup(override_content_type);

  status = clazz->initialize(object);
  if (status < 0)
	{
	  clazz->finalize(object);
	  PR_Free(object);
	  return 0;
	}

  return object;
}

void
mime_free (MimeObject *object)
{
# ifdef DEBUG__
  int i, size = object->clazz->instance_size;
  PRUint32 *array = (PRUint32*) object;
# endif /* DEBUG */

  object->clazz->finalize(object);

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
  MimeObjectClass *clazz = 0;
  MimeObjectClass *tempClass = 0;
  contentTypeHandlerInitStruct  ctHandlerInfo;

  /* 
  * What we do first is check for an external content handler plugin. 
  * This will actually extend the mime handling by calling a routine 
  * which will allow us to load an external content type handler
  * for specific content types. If one is not found, we will drop back
  * to the default handler.
  */
  if ((tempClass = mime_locate_external_content_handler(content_type, &ctHandlerInfo)) != NULL)
  {
    clazz = (MimeObjectClass *)tempClass;
  }
  else
  {
    if (!content_type || !*content_type ||
      !nsCRT::strcasecmp(content_type, "text"))  /* with no / in the type */
      clazz = (MimeObjectClass *)&mimeUntypedTextClass;
    
      /* Subtypes of text...
    */
    else if (!nsCRT::strncasecmp(content_type,			"text/", 5))
    {
      if      (!nsCRT::strcasecmp(content_type+5,		"html"))
        clazz = (MimeObjectClass *)&mimeInlineTextHTMLClass;
      else if (!nsCRT::strcasecmp(content_type+5,		"enriched"))
        clazz = (MimeObjectClass *)&mimeInlineTextEnrichedClass;
      else if (!nsCRT::strcasecmp(content_type+5,		"richtext"))
        clazz = (MimeObjectClass *)&mimeInlineTextRichtextClass;
      else if      (!nsCRT::strcasecmp(content_type+5,		"rtf"))
        clazz = (MimeObjectClass *)&mimeExternalObjectClass;
      else if (!nsCRT::strcasecmp(content_type+5,		"plain"))
      {
        // Preliminary use the normal plain text
        clazz = (MimeObjectClass *)&mimeInlineTextPlainClass;

        PRBool disable_format_flowed = PR_FALSE;
        nsIPref *pref = GetPrefServiceManager(opts); 
        if (pref)
          (void)pref->GetBoolPref(
                           "mailnews.display.disable_format_flowed_support",
                           &disable_format_flowed);
        
        if(!disable_format_flowed)
        {
          // Check for format=flowed, damn, it is already stripped away from
          // the contenttype!
          // Look in headers instead even though it's expensive and clumsy
          // First find Content-Type:
          char *content_type_row =
            (hdrs
             ? MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE,
                               PR_FALSE, PR_FALSE)
             : 0);
          // Then the format parameter if there is one.
          // I would rather use a PARAM_FORMAT but I can't find the right
          // place to put the define. The others seems to be in net.h
          // but is that really really the right place? There is also
          // a nsMimeTypes.h but that one isn't included. Bug?
          char *content_type_format =
            (content_type_row
             ? MimeHeaders_get_parameter(content_type_row, "format", NULL, NULL)
             : 0);

          if (content_type_format && !nsCRT::strcasecmp(content_type_format,
                                                           "flowed"))
            clazz = (MimeObjectClass *)&mimeInlineTextPlainFlowedClass;
          PR_FREEIF(content_type_format);
          PR_FREEIF(content_type_row);
        }
      }
      else if (!exact_match_p)
        clazz = (MimeObjectClass *)&mimeInlineTextPlainClass;
    }
    
    /* Subtypes of multipart...
    */
    else if (!nsCRT::strncasecmp(content_type,			"multipart/", 10))
    {
      if      (!nsCRT::strcasecmp(content_type+10,	"alternative"))
        clazz = (MimeObjectClass *)&mimeMultipartAlternativeClass;
      else if (!nsCRT::strcasecmp(content_type+10,	"related"))
        clazz = (MimeObjectClass *)&mimeMultipartRelatedClass;
      else if (!nsCRT::strcasecmp(content_type+10,	"digest"))
        clazz = (MimeObjectClass *)&mimeMultipartDigestClass;
      else if (!nsCRT::strcasecmp(content_type+10,	"appledouble") ||
        !nsCRT::strcasecmp(content_type+10,	"header-set"))
        clazz = (MimeObjectClass *)&mimeMultipartAppleDoubleClass;
      else if (!nsCRT::strcasecmp(content_type+10,	"parallel"))
        clazz = (MimeObjectClass *)&mimeMultipartParallelClass;
      else if (!nsCRT::strcasecmp(content_type+10,	"mixed"))
        clazz = (MimeObjectClass *)&mimeMultipartMixedClass;
#ifdef ENABLE_SMIME      
      else if (!nsCRT::strcasecmp(content_type+10,	"signed"))
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
        
          if (proto && (!nsCRT::strcasecmp(proto, APPLICATION_XPKCS7_SIGNATURE) &&
                          micalg && (!nsCRT::strcasecmp(micalg, PARAM_MICALG_MD5) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_2) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_3) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_4) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_5) ||
                                                 !nsCRT::strcasecmp(micalg, PARAM_MICALG_MD2))))
                        clazz = (MimeObjectClass *)&mimeMultipartSignedCMSClass;
                  else
                        clazz = 0;          
        PR_FREEIF(proto);
        PR_FREEIF(micalg);
        PR_FREEIF(ct);
      } 
#endif
      
      if (!clazz && !exact_match_p)
        /* Treat all unknown multipart subtypes as "multipart/mixed" */
        clazz = (MimeObjectClass *)&mimeMultipartMixedClass;
    }
    
    /* Subtypes of message...
    */
    else if (!nsCRT::strncasecmp(content_type,			"message/", 8))
    {
      if      (!nsCRT::strcasecmp(content_type+8,		"rfc822") ||
        !nsCRT::strcasecmp(content_type+8,		"news"))
        clazz = (MimeObjectClass *)&mimeMessageClass;
      else if (!nsCRT::strcasecmp(content_type+8,		"external-body"))
        clazz = (MimeObjectClass *)&mimeExternalBodyClass;
      else if (!nsCRT::strcasecmp(content_type+8,		"partial"))
        /* I guess these are most useful as externals, for now... */
        clazz = (MimeObjectClass *)&mimeExternalObjectClass;
      else if (!exact_match_p)
        /* Treat all unknown message subtypes as "text/plain" */
        clazz = (MimeObjectClass *)&mimeInlineTextPlainClass;
    }
    
    /* The magic image types which we are able to display internally...
    */
    else if (!nsCRT::strcasecmp(content_type,			IMAGE_GIF)  ||
      !nsCRT::strcasecmp(content_type,			IMAGE_JPG) ||
      !nsCRT::strcasecmp(content_type,			IMAGE_PJPG) ||
      !nsCRT::strcasecmp(content_type,			IMAGE_PNG) ||
      !nsCRT::strcasecmp(content_type,			IMAGE_XBM)  ||
      !nsCRT::strcasecmp(content_type,			IMAGE_XBM2) ||
      !nsCRT::strcasecmp(content_type,			IMAGE_XBM3))
      clazz = (MimeObjectClass *)&mimeInlineImageClass;
    
#ifdef ENABLE_SMIME
    else if (!nsCRT::strcasecmp(content_type, APPLICATION_XPKCS7_MIME))
	        clazz = (MimeObjectClass *)&mimeEncryptedCMSClass;
#endif
    /* A few types which occur in the real world and which we would otherwise
    treat as non-text types (which would be bad) without this special-case...
    */
    else if (!nsCRT::strcasecmp(content_type,			APPLICATION_PGP) ||
    !nsCRT::strcasecmp(content_type,			APPLICATION_PGP2))
    clazz = (MimeObjectClass *)&mimeInlineTextPlainClass;
    
    else if (!nsCRT::strcasecmp(content_type,			SUN_ATTACHMENT))
      clazz = (MimeObjectClass *)&mimeSunAttachmentClass;
    
      /* Everything else gets represented as a clickable link.
    */
    else if (!exact_match_p)
      clazz = (MimeObjectClass *)&mimeExternalObjectClass;  
  }

 if (!exact_match_p)
   PR_ASSERT(clazz);
  if (!clazz) return 0;

  PR_ASSERT(clazz);

  if (clazz && !clazz->class_initialized)
	{
	  int status = mime_classinit(clazz);
	  if (status < 0) return 0;
	}

  return clazz;
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

  MimeObjectClass *clazz = 0;
  char *content_disposition = 0;
  MimeObject *obj = 0;
  char *override_content_type = 0;
  static PRBool reverse_lookup = PR_FALSE, got_lookup_pref = PR_FALSE;

  if (!got_lookup_pref)
  {
     nsIPref *pref = GetPrefServiceManager(opts);   // Pref service manager 
     if (pref)
     {
       pref->GetBoolPref("mailnews.autolookup_unknown_mime_types",&reverse_lookup);
       got_lookup_pref = PR_TRUE;
     }
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
	  (content_type ? nsCRT::strcasecmp(content_type, APPLICATION_APPLEFILE) : PR_TRUE) &&
	  /* ## davidm Apple double shouldn't use this #$%& either. */
	  (content_type ? nsCRT::strcasecmp(content_type, MULTIPART_APPLEDOUBLE) : PR_TRUE) &&
	  (!content_type ||
	   !nsCRT::strcasecmp(content_type, APPLICATION_OCTET_STREAM) ||
	   !nsCRT::strcasecmp(content_type, UNKNOWN_CONTENT_TYPE) ||
	   (reverse_lookup
#if 0
	    && !NET_cinfo_find_info_by_type((char*)content_type))))
#else
        )))
#endif
	{
	  char *name = MimeHeaders_get_name(hdrs, opts);
	  if (name)
		{
		  override_content_type = opts->file_type_fn (name, opts->stream_closure);
		  PR_FREEIF(name);

      // Of, if we got here and it is not the unknown content type from the 
      // file name, lets do some better checking not to inline something bad
      //                      
      if (override_content_type && (nsCRT::strcasecmp(override_content_type, UNKNOWN_CONTENT_TYPE)))
      {
        // Only inline this if it makes sense to do so!
        if ( (!content_type) ||
             (content_type && (!nsCRT::strcasecmp(content_type, UNKNOWN_CONTENT_TYPE))) )
        {
  	  		content_type = override_content_type;
        }
        else
          PR_FREEIF(override_content_type);
      }
		}
	}

  clazz = mime_find_class(content_type, hdrs, opts, PR_FALSE);

  PR_ASSERT(clazz);
  if (!clazz) goto FAIL;

  if (opts && opts->part_to_load)
	/* Always ignore Content-Disposition when we're loading some specific
	   sub-part (which may be within some container that we wouldn't otherwise
	   descend into, if the container itself had a Content-Disposition of
	   `attachment'. */
	content_disposition = 0;

  else if (mime_subclass_p(clazz,(MimeObjectClass *)&mimeContainerClass) &&
		   !mime_subclass_p(clazz,(MimeObjectClass *)&mimeMessageClass))
	/* Ignore Content-Disposition on all containers except `message'.
	   That is, Content-Disposition is ignored for multipart/mixed objects,
	   but is obeyed for message/rfc822 objects. */
	content_disposition = 0;

  else
  {
    /* Check to see if the plugin should override the content disposition
       to make it appear inline. One example is a vcard which has a content
       disposition of an "attachment;" */
    if (force_inline_display(content_type))
  		NS_MsgSACopy(&content_disposition, "inline");
    else
  		content_disposition = (hdrs
							   ? MimeHeaders_get(hdrs, HEADER_CONTENT_DISPOSITION, PR_TRUE, PR_FALSE)
							   : 0);
  }

  if (!content_disposition || !nsCRT::strcasecmp(content_disposition, "inline"))
    ;	/* Use the class we've got. */
  else
  { 
    // 
    // rhp: Ok, this is a modification to try to deal with messages
    //      that have content disposition set to "attachment" even though
    //      we probably should show them inline. 
    //
    if (  (clazz != (MimeObjectClass *)&mimeInlineTextHTMLClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextPlainClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextPlainFlowedClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextHTMLClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextRichtextClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineTextEnrichedClass) &&
          (clazz != (MimeObjectClass *)&mimeMessageClass) &&
          (clazz != (MimeObjectClass *)&mimeInlineImageClass) )
      clazz = (MimeObjectClass *)&mimeExternalObjectClass;
  }

  PR_FREEIF(content_disposition);
  obj = mime_new (clazz, hdrs, content_type);

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



static int mime_classinit_1(MimeObjectClass *clazz, MimeObjectClass *target);

static int
mime_classinit(MimeObjectClass *clazz)
{
  int status;
  if (clazz->class_initialized)
	return 0;

  PR_ASSERT(clazz->class_initialize);
  if (!clazz->class_initialize)
	return -1;

  /* First initialize the superclass.
   */
  if (clazz->superclass && !clazz->superclass->class_initialized)
	{
	  status = mime_classinit(clazz->superclass);
	  if (status < 0) return status;
	}

  /* Now run each of the superclass-init procedures in turn,
	 parentmost-first. */
  status = mime_classinit_1(clazz, clazz);
  if (status < 0) return status;

  /* Now we're done. */
  clazz->class_initialized = PR_TRUE;
  return 0;
}

static int
mime_classinit_1(MimeObjectClass *clazz, MimeObjectClass *target)
{
  int status;
  if (clazz->superclass)
	{
	  status = mime_classinit_1(clazz->superclass, target);
	  if (status < 0) return status;
	}
  return clazz->class_initialize(target);
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
mime_typep(MimeObject *obj, MimeObjectClass *clazz)
{
  return mime_subclass_p(obj->clazz, clazz);
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
	return nsCRT::strdup("0");
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

	  PR_snprintf(buf, sizeof(buf), "%ld", j);
	  if (obj->parent->parent)
		{
		  higher = mime_part_address(obj->parent);
		  if (!higher) return 0;  /* MIME_OUT_OF_MEMORY */
		}

	  if (!higher)
		return nsCRT::strdup(buf);
	  else
		{
		  char *s = (char *)PR_MALLOC(nsCRT::strlen(higher) + nsCRT::strlen(buf) + 3);
		  if (!s)
			{
			  PR_Free(higher);
			  return 0;  /* MIME_OUT_OF_MEMORY */
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

#ifdef ENABLE_SMIME
/* Asks whether the given object is one of the cryptographically signed
   or encrypted objects that we know about.  (MimeMessageClass uses this
   to decide if the headers need to be presented differently.)
 */
PRBool
mime_crypto_object_p(MimeHeaders *hdrs, PRBool clearsigned_counts)
{
  char *ct;
  MimeObjectClass *clazz;

  if (!hdrs) return PR_FALSE;

  ct = MimeHeaders_get (hdrs, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);
  if (!ct) return PR_FALSE;

  /* Rough cut -- look at the string before doing a more complex comparison. */
  if (nsCRT::strcasecmp(ct, MULTIPART_SIGNED) &&
    nsCRT::strncasecmp(ct, "application/", 12))
	{
	  PR_Free(ct);
	  return PR_FALSE;
	}

  /* It's a candidate for being a crypto object.  Let's find out for sure... */
  clazz = mime_find_class (ct, hdrs, 0, PR_TRUE);
  PR_Free(ct);

  if (clazz == ((MimeObjectClass *)&mimeEncryptedCMSClass))
	return PR_TRUE;
  else if (clearsigned_counts &&
		   clazz == ((MimeObjectClass *)&mimeMultipartSignedCMSClass))
	return PR_TRUE;
  else
	return PR_FALSE;
}

/* Whether the given object has written out the HTML version of its headers
   in such a way that it will have a "crypto stamp" next to the headers.  If
   this is true, then the child must write out its HTML slightly differently
   to take this into account...
 */
PRBool
mime_crypto_stamped_p(MimeObject *obj)
{
  if (!obj) return PR_FALSE;
  if (mime_typep (obj, (MimeObjectClass *) &mimeMessageClass))
	return ((MimeMessage *) obj)->crypto_stamped_p;
  else
	return PR_FALSE;
}


/* Tells whether the given MimeObject is a message which has been encrypted
   or signed.  (Helper for MIME_GetMessageCryptoState()). 
 */
void
mime_get_crypto_state (MimeObject *obj,
					   PRBool *signed_ret,
					   PRBool *encrypted_ret,
					   PRBool *signed_ok_ret,
					   PRBool *encrypted_ok_ret)
{
  PRBool signed_p, encrypted_p;

  if (signed_ret) *signed_ret = PR_FALSE;
  if (encrypted_ret) *encrypted_ret = PR_FALSE;
  if (signed_ok_ret) *signed_ok_ret = PR_FALSE;
  if (encrypted_ok_ret) *encrypted_ok_ret = PR_FALSE;

  PR_ASSERT(obj);
  if (!obj) return;

  if (!mime_typep (obj, (MimeObjectClass *) &mimeMessageClass))
	return;

  signed_p = ((MimeMessage *) obj)->crypto_msg_signed_p;
  encrypted_p = ((MimeMessage *) obj)->crypto_msg_encrypted_p;

  if (signed_ret)
	*signed_ret = signed_p;
  if (encrypted_ret)
	*encrypted_ret = encrypted_p;

  if ((signed_p || encrypted_p) &&
	  (signed_ok_ret || encrypted_ok_ret))
	{
	  nsICMSMessage *encrypted_ci = 0;
	  nsICMSMessage *signed_ci = 0;
	  PRInt32 decode_error = 0, verify_error = 0;
	  char *addr = mime_part_address(obj);

	  mime_find_security_info_of_part(addr, obj,
									  &encrypted_ci,
									  &signed_ci,
									  0,  /* email_addr */
									  &decode_error, &verify_error);

	  if (encrypted_p && encrypted_ok_ret)
		*encrypted_ok_ret = (encrypted_ci && decode_error >= 0);

	  if (signed_p && signed_ok_ret)
		*signed_ok_ret = (verify_error >= 0 && decode_error >= 0);

	  PR_FREEIF(addr);
	}
}


/* How the crypto code tells the MimeMessage object what the crypto stamp
   on it says. */
void
mime_set_crypto_stamp(MimeObject *obj, PRBool signed_p, PRBool encrypted_p)
{
  if (!obj) return;
  if (mime_typep (obj, (MimeObjectClass *) &mimeMessageClass))
	{
	  MimeMessage *msg = (MimeMessage *) obj;
	  if (!msg->crypto_msg_signed_p)
		msg->crypto_msg_signed_p = signed_p;
	  if (!msg->crypto_msg_encrypted_p)
		msg->crypto_msg_encrypted_p = encrypted_p;

	  /* If the `decrypt_p' option is on, record whether any decryption has
		 actually occurred. */
	  if (encrypted_p &&
		  obj->options &&
		  obj->options->decrypt_p &&
		  obj->options->state)
		{
		  /* decrypt_p and write_html_p are incompatible. */
		  PR_ASSERT(!obj->options->write_html_p);
		  obj->options->state->decrypted_p = PR_TRUE;
    }

    return;  /* continue up the tree?  I think that's not a good idea. */
  }

  if (obj->parent)
    mime_set_crypto_stamp(obj->parent, signed_p, encrypted_p);
}

/* Given a part ID, looks through the MimeObject tree for a sub-part whose ID
   number matches; if one is found, and if it represents a PKCS7-encrypted
   object, returns information about the security status of that object.

   `part' is not a URL -- it's of the form "1.3.5" and is interpreted relative
   to the `obj' argument.

   Returned values:

     void **pkcs7_content_info_return
          this is the SEC_PKCS7ContentInfo* of the object.

     int32 *decode_error_return
          this is the error code, if any, that the security library returned
	      while trying to parse the PKCS7 data (if this is negative, then it
		  probably means the message was corrupt in some way.)

     int32 *verify_error_return
          this is the error code, if any, that the security library returned
  	      while trying to decrypt or verify or otherwise validate the data
		  (if this is negative, it might mean the message was corrupt, or might
		  mean the signature didn't match, or the cert was expired, or...)
  */
void
mime_find_security_info_of_part(const char *part, MimeObject *obj,
								nsICMSMessage **pkcs7_encrypted_content_info_return,
								nsICMSMessage **pkcs7_signed_content_info_return,
								char **sender_email_addr_return,
								PRInt32 *decode_error_return,
								PRInt32 *verify_error_return)
{
  obj = mime_address_to_part(part, obj);

  *pkcs7_encrypted_content_info_return = 0;
  *pkcs7_signed_content_info_return = 0;
  *decode_error_return = 0;
  *verify_error_return = 0;
  if (sender_email_addr_return)
	*sender_email_addr_return = 0;

  if (!obj)
	return;

  /* If someone asks for the security info of a message/rfc822 object,
	 instead give them the security info of its child (the body of the
	 message.)
   */
  if (mime_typep (obj, (MimeObjectClass *) &mimeMessageClass))
	{
	  MimeContainer *cont = (MimeContainer *) obj;
	  if (cont->nchildren >= 1)
		{
		  PR_ASSERT(cont->nchildren == 1);
		  obj = cont->children[0];
		}
	}


  while (obj &&
		 (mime_typep(obj, (MimeObjectClass *) &mimeEncryptedCMSClass) ||
		  mime_typep(obj, (MimeObjectClass *) &mimeMultipartSignedCMSClass)))
	{
	  nsICMSMessage *ci = 0;
	  PRInt32 decode_error = 0, verify_error = 0;
      PRBool ci_is_encrypted = PR_FALSE;
	  char *sender = 0;

	  if (mime_typep(obj, (MimeObjectClass *) &mimeEncryptedCMSClass)) {
		(((MimeEncryptedCMSClass *) (obj->clazz))
		 ->get_content_info) (obj, &ci, &sender, &decode_error, &verify_error, &ci_is_encrypted);
      } else if (mime_typep(obj,
						  (MimeObjectClass *) &mimeMultipartSignedCMSClass)) {
		(((MimeMultipartSignedCMSClass *) (obj->clazz))
		 ->get_content_info) (obj, &ci, &sender, &decode_error, &verify_error, &ci_is_encrypted);
      }

      if (ci) {
        if (ci_is_encrypted) {
            *pkcs7_encrypted_content_info_return = ci;
        } else {
            *pkcs7_signed_content_info_return = ci;
        }
      }

      if (sender_email_addr_return)
		*sender_email_addr_return = sender;
	  else
		PR_FREEIF(sender);

	  if (*decode_error_return >= 0)
		*decode_error_return = decode_error;

	  if (*verify_error_return >= 0)
		*verify_error_return = verify_error;


	  PR_ASSERT(mime_typep(obj, (MimeObjectClass *) &mimeContainerClass) &&
				((MimeContainer *) obj)->nchildren <= 1);

	  obj = ((((MimeContainer *) obj)->nchildren > 0)
			 ? ((MimeContainer *) obj)->children[0]
			 : 0);
	}
}

#endif // ENABLE_SMIME

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

  if (!url || !part) return 0;

  for (s = url; *s; s++)
	{
	  if (*s == '?')
		{
		  got_q = PR_TRUE;
		  if (!nsCRT::strncasecmp(s, "?part=", 6))
			part_begin = (s += 6);
		}
	  else if (got_q && *s == '&' && !nsCRT::strncasecmp(s, "&part=", 6))
		part_begin = (s += 6);

	  if (part_begin)
		{
		  for (; (*s && *s != '?' && *s != '&'); s++)
			;
		  part_end = s;
		  break;
		}
	}

  result = (char *) PR_MALLOC(nsCRT::strlen(url) + nsCRT::strlen(part) + 10);
  if (!result) return 0;

  if (part_begin)
	{
	  if (append_p)
		{
		  nsCRT::memcpy(result, url, part_end - url);
		  result [part_end - url]     = '.';
		  result [part_end - url + 1] = 0;
		}
	  else
		{
		  nsCRT::memcpy(result, url, part_begin - url);
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
	int L = nsCRT::strlen(result);
	if (L > 6 &&
		(result[L-7] == '?' || result[L-7] == '&') &&
		!nsCRT::strcmp("part=0", result + L - 6))
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
	
  result = (char *) PR_MALLOC(nsCRT::strlen(url) + nsCRT::strlen(imappart) + nsCRT::strlen(libmimepart) + 17);
  if (!result) return 0;

  PL_strcpy(result, url);
  PL_strcat(result, "/;section=");
  PL_strcat(result, imappart);
  PL_strcat(result, "&part=");
  PL_strcat(result, libmimepart);
  result[nsCRT::strlen(result)] = 0;

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
	  if (!part2) return 0;  /* MIME_OUT_OF_MEMORY */
	  match = !nsCRT::strcmp(part, part2);
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

  result = (obj->headers ? MimeHeaders_get_name(obj->headers, obj->options) : 0);

  /* If this part doesn't have a name, but this part is one fork of an
	 AppleDouble, and the AppleDouble itself has a name, then use that. */
  if (!result &&
	  obj->parent &&
	  obj->parent->headers &&
	  mime_typep(obj->parent,
				 (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	result = MimeHeaders_get_name(obj->parent->headers, obj->options);

  /* Else, if this part is itself an AppleDouble, and one of its children
	 has a name, then use that (check data fork first, then resource.) */
  if (!result &&
	  mime_typep(obj, (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	{
	  MimeContainer *cont = (MimeContainer *) obj;
	  if (cont->nchildren > 1 &&
		  cont->children[1] &&
		  cont->children[1]->headers)
		result = MimeHeaders_get_name(cont->children[1]->headers, obj->options);

	  if (!result &&
		  cont->nchildren > 0 &&
		  cont->children[0] &&
		  cont->children[0]->headers)
		result = MimeHeaders_get_name(cont->children[0]->headers, obj->options);
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
	  PRInt32 L = nsCRT::strlen(result);
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
	  if (!nsCRT::strcasecmp(obj->encoding, ENCODING_UUENCODE))
		{
		  static const char *uue_exts[] = { "uu", "uue", 0 };
		  exts = uue_exts;
		}

	  while (exts && *exts)
		{
		  const char *ext = *exts;
		  PRInt32 L2 = nsCRT::strlen(ext);
		  if (L > L2 + 1 &&							/* long enough */
			  result[L - L2 - 1] == '.' &&			/* '.' in right place*/
			  !nsCRT::strcasecmp(ext, result + (L - L2)))	/* ext matches */
			{
			  result[L - L2 - 1] = 0;		/* truncate at '.' and stop. */
			  break;
			}
		  exts++;
		}
	}

  return result;
}

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
	  else if (!nsCRT::strncasecmp ("headers", q, name_end - q))
		{
      if (end > value && !nsCRT::strncasecmp ("only", value, end-value))
        options->headers = MimeHeadersOnly;
      else if (end > value && !nsCRT::strncasecmp ("none", value, end-value))
        options->headers = MimeHeadersNone;      
      else if (end > value && !nsCRT::strncasecmp ("all", value, end - value))
        options->headers = MimeHeadersAll;
      else if (end > value && !nsCRT::strncasecmp ("some", value, end - value))
        options->headers = MimeHeadersSome;
      else if (end > value && !nsCRT::strncasecmp ("micro", value, end - value))
        options->headers = MimeHeadersMicro;
      else if (end > value && !nsCRT::strncasecmp ("cite", value, end - value))
        options->headers = MimeHeadersCitation;
      else if (end > value && !nsCRT::strncasecmp ("citation", value, end-value))
        options->headers = MimeHeadersCitation;
      else
        options->headers = default_headers;
		}
	  else if (!nsCRT::strncasecmp ("part", q, name_end - q))
		{
		  PR_FREEIF (options->part_to_load);
		  if (end > value)
			{
			  options->part_to_load = (char *) PR_MALLOC(end - value + 1);
			  if (!options->part_to_load)
				return MIME_OUT_OF_MEMORY;
			  nsCRT::memcpy(options->part_to_load, value, end-value);
			  options->part_to_load[end-value] = 0;
			}
		}
	  else if (!nsCRT::strncasecmp ("rot13", q, name_end - q))
		{
		  if (end <= value || !nsCRT::strncasecmp ("true", value, end - value))
			options->rot13_p = PR_TRUE;
		  else
			options->rot13_p = PR_FALSE;
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
	  if (!nsCRT::strcmp(options->part_to_load, "0"))		/* 0 */
		{
		  PR_Free(options->part_to_load);
		  options->part_to_load = nsCRT::strdup("1");
		  if (!options->part_to_load)
			return MIME_OUT_OF_MEMORY;
		}
	  else if (nsCRT::strcmp(options->part_to_load, "1"))	/* not 1 */
		{
		  const char *prefix = "1.";
		  char *s = (char *) PR_MALLOC(nsCRT::strlen(options->part_to_load) +
									  nsCRT::strlen(prefix) + 1);
		  if (!s) return MIME_OUT_OF_MEMORY;
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

//  PR_ASSERT(opt->state->first_data_written_p);

  if (opt->state->separator_queued_p && user_visible_p)
	{
	  opt->state->separator_queued_p = PR_FALSE;
	  if (opt->state->separator_suppressed_p)
		opt->state->separator_suppressed_p = PR_FALSE;
	  else
		{
		  char sep[] = "<BR><HR WIDTH=\"90%\" SIZE=4><BR>";
		  int lstatus = opt->output_fn(sep, nsCRT::strlen(sep), closure);
		  opt->state->separator_suppressed_p = PR_FALSE;
		  if (lstatus < 0) return lstatus;
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
		  name = MimeHeaders_get_name(obj->headers, obj->options);

		  ct = MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE,
							   PR_FALSE, PR_FALSE);
		  if (ct)
			{
			  x_mac_type   = MimeHeaders_get_parameter(ct, PARAM_X_MAC_TYPE, NULL, NULL);
			  x_mac_creator= MimeHeaders_get_parameter(ct, PARAM_X_MAC_CREATOR, NULL, NULL);
        /* if don't have a x_mac_type and x_mac_creator, we need to try to get it from its parent */
        if (!x_mac_type && !x_mac_creator && obj->parent && obj->parent->headers)
        {
          char * ctp = MimeHeaders_get(obj->parent->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
          if (ctp)
          {
            x_mac_type   = MimeHeaders_get_parameter(ctp, PARAM_X_MAC_TYPE, NULL, NULL);
            x_mac_creator= MimeHeaders_get_parameter(ctp, PARAM_X_MAC_CREATOR, NULL, NULL);
            PR_Free(ctp);
          }
        }
			  
        if (!(obj->options->override_charset)) {
          PR_FREEIF(obj->options->default_charset);
          obj->options->default_charset = MimeHeaders_get_parameter(ct, "charset", nsnull, nsnull);
        }
			  PR_Free(ct);
			}
		}

	  if (mime_typep(obj, (MimeObjectClass *) &mimeInlineTextClass))
		charset = ((MimeInlineText *)obj)->charset;

	  if (!content_type)
		content_type = obj->content_type;
	  if (!content_type)
		content_type = TEXT_PLAIN;

    // 
    // Set the charset on the channel we are dealing with so people know 
    // what the charset is set to. Do this for quoting/Printing ONLY! 
    // 
    extern void   PR_CALLBACK ResetChannelCharset(MimeObject *obj);
    if ( (obj->options) && 
         ( (obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting) || 
           (obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting) || 
           (obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs) || 
           (obj->options->format_out == nsMimeOutput::nsMimeMessagePrintOutput) ) ) 
      ResetChannelCharset(obj); 

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

