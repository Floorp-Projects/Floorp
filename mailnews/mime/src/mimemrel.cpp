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

/* Thoughts on how to implement this:

   = if the type of this multipart/related is not text/html, then treat
     it the same as multipart/mixed.

   = For each part in this multipart/related
     = if this part is not the "top" part
       = then save this part to a tmp file or a memory object,
         kind-of like what we do for multipart/alternative sub-parts.
         If this is an object we're blocked on (see below) send its data along.
     = else
       = emit this part (remember, it's of type text/html)
       = at some point, layout may load a URL for <IMG SRC="cid:xxxx">.
         we intercept that.
         = if one of our cached parts has that cid, return the data for it.
         = else, "block", the same way the image library blocks layout when it
           doesn't yet have the size of the image.
       = at some point, layout may load a URL for <IMG SRC="relative/yyy">.
         we need to intercept that too.
         = expand the URL, and compare it to our cached objects.
           if it matches, return it.
         = else block on it.

   = once we get to the end, if we have any sub-part references that we're
     still blocked on, map over them:
     = if they're cid: references, close them ("broken image" results.)
     = if they're URLs, then load them in the normal way.

  --------------------------------------------------

  Ok, that's fairly complicated.  How about an approach where we go through
  all the parts first, and don't emit until the end?

   = if the type of this multipart/related is not text/html, then treat
     it the same as multipart/mixed.

   = For each part in this multipart/related
       = save this part to a tmp file or a memory object,
         like what we do for multipart/alternative sub-parts.

   = Emit the "top" part (the text/html one)
     = intercept all calls to NET_GetURL, to allow us to rewrite the URL.
       (hook into netlib, or only into imglib's calls to GetURL?)
       (make sure we're behaving in a context-local way.)

       = when a URL is loaded, look through our cached parts for a match.
         = if we find one, map that URL to a "cid:" URL
         = else, let it load normally

     = at some point, layout may load a URL for <IMG SRC="cid:xxxx">.
       it will do this either because that's what was in the HTML, or because
       that's how we "rewrote" the URLs when we intercepted NET_GetURL.

       = if one of our cached parts has the requested cid, return the data
         for it.
       = else, generate a "broken image"

   = free all the cached data

  --------------------------------------------------

  How hard would be an approach where we rewrite the HTML?
  (Looks like it's not much easier, and might be more error-prone.)

   = if the type of this multipart/related is not text/html, then treat
     it the same as multipart/mixed.

   = For each part in this multipart/related
       = save this part to a tmp file or a memory object,
         like what we do for multipart/alternative sub-parts.

   = Parse the "top" part, and emit slightly different HTML:
     = for each <IMG SRC>, <IMG LOWSRC>, <A HREF>?  Any others?
       = look through our cached parts for a matching URL
         = if we find one, map that URL to a "cid:" URL
         = else, let it load normally

     = at some point, layout may load a URL for <IMG SRC="cid:xxxx">.
       = if one of our cached parts has the requested cid, return the data
         for it.
       = else, generate a "broken image"

   = free all the cached data
 */
#include "nsCOMPtr.h"
#include "mimemrel.h"
#include "prmem.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include "mimemoz2.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"
#include "nsMimeTypes.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "mimebuf.h"
#include "nsMsgUtils.h"

//
// External Defines...
//
extern nsFileSpec *nsMsgCreateTempFileSpec(char *tFileName);

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeMultipartRelated, MimeMultipartRelatedClass,
			 mimeMultipartRelatedClass, &MIME_SUPERCLASS);


/* Stupid utility function.  Really ought to be part of the standard string
   package if you ask me...*/

static char* mime_strnchr(char* str, char c, int length)
{
	int i;
	for (i=0 ; i<length ; i++) {
		if (*str == c) return str;
		str++;
	}
	return NULL;
}


static int
MimeMultipartRelated_initialize(MimeObject* obj)
{
	MimeMultipartRelated* relobj = (MimeMultipartRelated*) obj;
	relobj->base_url = MimeHeaders_get(obj->headers, HEADER_CONTENT_BASE,
									   PR_FALSE, PR_FALSE);
  /* rhp: need this for supporting Content-Location */
  if (!relobj->base_url)
  {
    relobj->base_url = MimeHeaders_get(obj->headers, HEADER_CONTENT_LOCATION,
      PR_FALSE, PR_FALSE);
  }
  /* rhp: need this for supporting Content-Location */

	/* I used to have code here to test if the type was text/html.  Then I
	   added multipart/alternative as being OK, too.  Then I found that the
	   VCard spec seems to talk about having the first part of a
	   multipart/related be an application/directory.  At that point, I decided
	   to punt.  We handle anything as the first part, and stomp on the HTML it
	   generates to adjust tags to point into the other parts.  This probably
	   works out to something reasonable in most cases. */

  relobj->hash = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues,
                                (PLHashAllocOps *)NULL, NULL);

	if (!relobj->hash) return MIME_OUT_OF_MEMORY;

  relobj->input_file_stream = nsnull;
  relobj->output_file_stream = nsnull;

	return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(obj);
}

static PRIntn PR_CALLBACK 
mime_multipart_related_nukehash(PLHashEntry *table, 
          				       						   PRIntn indx, void *arg)                             
{
  if (table->key)
    PR_Free((char*) table->key);

  if (table->value)
    PR_Free((char*) table->value);
	return HT_ENUMERATE_NEXT;		/* XP_Maphash will continue traversing the hash */
}

static void
MimeMultipartRelated_finalize (MimeObject *obj)
{
	MimeMultipartRelated* relobj = (MimeMultipartRelated*) obj;
	PR_FREEIF(relobj->base_url);
	PR_FREEIF(relobj->curtag);
        PR_FREEIF(relobj->head_buffer);
        relobj->head_buffer_fp = 0;
        relobj->head_buffer_size = 0;
	if (relobj->hash) {
    PL_HashTableEnumerateEntries(relobj->hash, mime_multipart_related_nukehash, NULL);
    PL_HashTableDestroy(relobj->hash);
		relobj->hash = NULL;
	}

	if (relobj->input_file_stream) 
  {
		relobj->input_file_stream->close();
    delete relobj->input_file_stream;
    relobj->input_file_stream = nsnull;
	}

  if (relobj->output_file_stream) 
  {
		relobj->output_file_stream->close();
    delete relobj->output_file_stream;
    relobj->output_file_stream = nsnull;
	}

	if (relobj->file_buffer_spec) 
  {
    relobj->file_buffer_spec->Delete(PR_FALSE);
    delete relobj->file_buffer_spec;
    relobj->file_buffer_spec = nsnull;
	}

	((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(obj);
}

#define ISHEX(c) ( ((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F') )
#define NONHEX(c) (!ISHEX(c))

extern "C" char * 
escape_unescaped_percents(const char *incomingURL)
{
	const char *inC;
	char *outC;
	char *result = (char *) PR_Malloc(nsCRT::strlen(incomingURL)*3+1);

	if (result)
	{
		for(inC = incomingURL, outC = result; *inC != '\0'; inC++)
		{
			if (*inC == '%')
			{
				/* Check if either of the next two characters are non-hex. */
				if ( !*(inC+1) || NONHEX(*(inC+1)) || !*(inC+2) || NONHEX(*(inC+2)) )
				{
					/* Hex characters don't follow, escape the 
					   percent char */
					*outC++ = '%'; *outC++ = '2'; *outC++ = '5';
				}
				else
				{
					/* Hex characters follow, so assume the percent 
					   is escaping something else */
					*outC++ = *inC;
				}
			}
			else
				*outC++ = *inC;
		}
		*outC = '\0';
	}

	return result;
}

/* This routine is only necessary because the mailbox URL fed to us 
   by the winfe can contain spaces and '>'s in it. It's a hack. */
static char *
escape_for_mrel_subst(char *inURL)
{
	char *output, *inC, *outC, *temp;

	/* nsCRT::strlen asserts the presence of a string in inURL */
	int size = nsCRT::strlen(inURL) + 1;

	for(inC = inURL; *inC; inC++)
		if ((*inC == ' ') || (*inC == '>')) 
			size += 2; /* space -> '%20', '>' -> '%3E', etc. */

	output = (char *)PR_MALLOC(size);
	if (output)
	{
		/* Walk through the source string, copying all chars
			except for spaces, which get escaped. */
		inC = inURL;
		outC = output;
		while(*inC)
		{
			if (*inC == ' ')
			{
				*outC++ = '%'; *outC++ = '2'; *outC++ = '0';
			}
			else if (*inC == '>')
			{
				*outC++ = '%'; *outC++ = '3'; *outC++ = 'E';
			}
			else
				*outC++ = *inC;

			inC++;
		}
		*outC = '\0';
	
		temp = escape_unescaped_percents(output);
		if (temp)
		{
			PR_FREEIF(output);
			output = temp;
		}
	}
	return output;
}

static PRBool
MimeStartParamExists(MimeObject *obj, MimeObject* child)
{
  char *ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  char *st = (ct
              ? MimeHeaders_get_parameter(ct, HEADER_PARM_START, NULL, NULL)
              : 0);
  if (!st)
    return PR_FALSE;

  PR_FREEIF(st);
  PR_FREEIF(ct);
  return PR_TRUE;
}

static PRBool
MimeThisIsStartPart(MimeObject *obj, MimeObject* child)
{
  PRBool rval = PR_FALSE;
  char *ct, *st, *cst;

  ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  st = (ct
        ? MimeHeaders_get_parameter(ct, HEADER_PARM_START, NULL, NULL)
        : 0);
  if (!st)
    return PR_FALSE;

  cst = MimeHeaders_get(child->headers, HEADER_CONTENT_ID, PR_FALSE, PR_FALSE);
  if (!cst)
    rval = PR_FALSE;
  else
  {
		char *tmp = cst;
    if (*tmp == '<') 
    {
      int length;
      tmp++;
      length = nsCRT::strlen(tmp);
      if (length > 0 && tmp[length - 1] == '>') 
      {
        tmp[length - 1] = '\0';
      }
    }

    rval = (!nsCRT::strcmp(st, tmp));
  }

  PR_FREEIF(st);
  PR_FREEIF(ct);
  PR_FREEIF(cst);
  return rval;
}
/* rhp - gotta support the "start" parameter */

char *
MakeAbsoluteURL(char *base_url, char *relative_url)
{
  char            *retString = nsnull;
  nsIURI          *base = nsnull;

  // if either is NULL, just return the relative if safe...
  if (!base_url || !relative_url)
  {
    if (!relative_url)
      return nsnull;

    NS_MsgSACopy(&retString, relative_url);
    return retString;
  }

  nsresult err = nsMimeNewURI(&base, base_url, nsnull);
  if (err != NS_OK) 
    return nsnull;
  
  nsIURI    *url = nsnull;
  err = nsMimeNewURI(&url, relative_url, base);
  if (err != NS_OK) 
    goto done;
  
  err = url->GetSpec(&retString);
  if (err)
  {
    retString = nsnull;
    goto done;
  }
  
done:
  NS_IF_RELEASE(url);
  NS_IF_RELEASE(base);
  return retString;
}

static PRBool
MimeMultipartRelated_output_child_p(MimeObject *obj, MimeObject* child)
{
	MimeMultipartRelated *relobj = (MimeMultipartRelated *) obj;

  /* rhp - Changed from "if (relobj->head_loaded)" alone to support the 
           start parameter
   */
  if (
       (relobj->head_loaded) || 
       (MimeStartParamExists(obj, child) && !MimeThisIsStartPart(obj, child)) 
     )
  {
		/* This is a child part.  Just remember the mapping between the URL
		   it represents and the part-URL to get it back. */
		char* location = MimeHeaders_get(child->headers, HEADER_CONTENT_LOCATION,
										 PR_FALSE, PR_FALSE);
    if (!location) {
			char* tmp = MimeHeaders_get(child->headers, HEADER_CONTENT_ID,
										PR_FALSE, PR_FALSE);
			if (tmp) {
				char* tmp2 = tmp;
				if (*tmp2 == '<') {
					int length;
					tmp2++;
					length = nsCRT::strlen(tmp2);
					if (length > 0 && tmp2[length - 1] == '>') {
						tmp2[length - 1] = '\0';
					}
				}
				location = PR_smprintf("cid:%s", tmp2);
				PR_Free(tmp);
			}
    }

 		if (location) {
      char *absolute;
			char *base_url = MimeHeaders_get(child->headers, HEADER_CONTENT_BASE,
											 PR_FALSE, PR_FALSE);
      /* rhp: need this for supporting Content-Location */
      if (!base_url)
      {
        base_url = MimeHeaders_get(child->headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE);
      }
      /* rhp: need this for supporting Content-Location */

      absolute = MakeAbsoluteURL(base_url ? base_url : relobj->base_url, location);

			PR_FREEIF(base_url);
			PR_Free(location);
			if (absolute) {
				char* partnum = mime_part_address(child);
				if (partnum) {
					char* part;
					part = mime_set_url_part(obj->options->url, partnum,
											 PR_FALSE);
					if (part) {
					    /* If there's a space in the url, escape the url.
						   (This happens primarily on Windows and Unix.) */
					    char *temp = part;
					    if (PL_strchr(part, ' ') || PL_strchr(part, '>') || PL_strchr(part, '%'))
						  temp = escape_for_mrel_subst(part);
            PL_HashTableAdd(relobj->hash, absolute, temp);

            /* rhp - If this part ALSO has a Content-ID we need to put that into
                     the hash table and this is what this code does
             */
            {
              char *tloc;
              char *tmp = MimeHeaders_get(child->headers, HEADER_CONTENT_ID, PR_FALSE, PR_FALSE);
              if (tmp) 
              {
                char* tmp2 = tmp;
                if (*tmp2 == '<') 
                {
                  int length;
                  tmp2++;
                  length = nsCRT::strlen(tmp2);
                  if (length > 0 && tmp2[length - 1] == '>') 
                  {
                    tmp2[length - 1] = '\0';
                  }
                }
                
                tloc = PR_smprintf("cid:%s", tmp2);
                PR_Free(tmp);
                if (tloc)
                {
                  PL_HashTableAdd(relobj->hash, tloc, nsCRT::strdup(temp));
                }
              }
            }
            /*  rhp - End of putting more stuff into the hash table */

						/* The value string that is added to the hashtable will be deleted
						   by the hashtable at destruction time. So if we created an
						   escaped string for the hashtable, we have to delete the
						   part URL that was given to us. */
						if (temp != part) PR_Free(part);
					}
					PR_Free(partnum);
				}
			}
		}
	} else {
		/* Ah-hah!  We're the head object.  */
		char* base_url;
		relobj->head_loaded = PR_TRUE;
		relobj->headobj = child;
    relobj->buffered_hdrs = MimeHeaders_copy(child->headers);
		base_url = MimeHeaders_get(child->headers, HEADER_CONTENT_BASE,
								   PR_FALSE, PR_FALSE);
    /* rhp: need this for supporting Content-Location */
    if (!base_url)
    {
      base_url = MimeHeaders_get(child->headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE);
    }
    /* rhp: need this for supporting Content-Location */

		if (base_url) {
			/* If the head object has a base_url associated with it, use
			   that instead of any base_url that may have been associated
			   with the multipart/related. */
			PR_FREEIF(relobj->base_url);
			relobj->base_url = base_url;
		}

	}
	if (obj->options && !obj->options->write_html_p
#ifdef MIME_DRAFTS
		&& !obj->options->decompose_file_p
#endif /* MIME_DRAFTS */
		)
	  {
		return PR_TRUE;
	  }

	return PR_FALSE;			/* Don't actually parse this child; we'll handle
							   all that at eof time. */
}

static int
MimeMultipartRelated_parse_child_line (MimeObject *obj,
									   char *line, PRInt32 length,
									   PRBool first_line_p)
{
	MimeContainer *cont = (MimeContainer *) obj;
	MimeMultipartRelated *relobj = (MimeMultipartRelated *) obj;
	int status;
	MimeObject *kid;

	if (obj->options && !obj->options->write_html_p
#ifdef MIME_DRAFTS
		&& !obj->options->decompose_file_p
#endif /* MIME_DRAFTS */
		)
	  {
		/* Oh, just go do the normal thing... */
		return ((MimeMultipartClass*)&MIME_SUPERCLASS)->
			parse_child_line(obj, line, length, first_line_p);
	  }

	/* Throw it away if this isn't the head object.  (Someday, maybe we'll
	   cache it instead.) */
	PR_ASSERT(cont->nchildren > 0);
	if (cont->nchildren <= 0)
		return -1;
	kid = cont->children[cont->nchildren-1];
	PR_ASSERT(kid);
	if (!kid) return -1;
	if (kid != relobj->headobj) return 0;

	/* Buffer this up (###tw much code duplication from mimemalt.c) */
	/* If we don't yet have a buffer (either memory or file) try and make a
	   memory buffer. */
	if (!relobj->head_buffer && !relobj->file_buffer_spec) {
		int target_size = 1024 * 50;       /* try for 50k */
		while (target_size > 0) {
			relobj->head_buffer = (char *) PR_MALLOC(target_size);
			if (relobj->head_buffer) break;  /* got it! */
			target_size -= (1024 * 5);     /* decrease it and try again */
		}

		if (relobj->head_buffer) {
			relobj->head_buffer_size = target_size;
		} else {
			relobj->head_buffer_size = 0;
		}

		relobj->head_buffer_fp = 0;
	}

	/* Ok, if at this point we still don't have either kind of buffer, try and
	   make a file buffer. */
	if (!relobj->head_buffer && !relobj->file_buffer_spec) 
  {
		relobj->file_buffer_spec = nsMsgCreateTempFileSpec("nsma");
		if (!relobj->file_buffer_spec) 
      return MIME_OUT_OF_MEMORY;

    relobj->output_file_stream = new nsOutputFileStream(*(relobj->file_buffer_spec), PR_WRONLY | PR_CREATE_FILE, 00600);
		if (!relobj->output_file_stream) 
    {
			return MIME_UNABLE_TO_OPEN_TMP_FILE;
		}
	}
  
	PR_ASSERT(relobj->head_buffer || relobj->output_file_stream);


	/* If this line will fit in the memory buffer, put it there.
	 */
	if (relobj->head_buffer &&
	    relobj->head_buffer_fp + length < relobj->head_buffer_size) {
		nsCRT::memcpy(relobj->head_buffer + relobj->head_buffer_fp, line, length);
		relobj->head_buffer_fp += length;
	} else {
		/* Otherwise it won't fit; write it to the file instead. */

		/* If the file isn't open yet, open it, and dump the memory buffer
		   to it. */
		if (!relobj->output_file_stream) 
    {
			if (!relobj->file_buffer_spec) 
      {
        relobj->file_buffer_spec = nsMsgCreateTempFileSpec("nsma");
			}

			if (!relobj->file_buffer_spec) 
        return MIME_OUT_OF_MEMORY;

      relobj->output_file_stream = new nsOutputFileStream(*(relobj->file_buffer_spec), PR_WRONLY | PR_CREATE_FILE, 00600);
			if (!relobj->output_file_stream) 
      {
				return MIME_UNABLE_TO_OPEN_TMP_FILE;
			}

			if (relobj->head_buffer && relobj->head_buffer_fp) 
      {
				status = relobj->output_file_stream->write(relobj->head_buffer,
							                                     relobj->head_buffer_fp);
				if (status < relobj->head_buffer_fp)
          return MIME_UNABLE_TO_OPEN_TMP_FILE;
			}

			PR_FREEIF(relobj->head_buffer);
			relobj->head_buffer_fp = 0;
			relobj->head_buffer_size = 0;
		}

		/* Dump this line to the file. */
    status = relobj->output_file_stream->write(line, length);
		if (status < length) 
      return status;
	}

	return 0;
}




static int
real_write(MimeMultipartRelated* relobj, char* buf, PRInt32 size)
{
  MimeObject* obj = (MimeObject*) relobj;
  void* closure = relobj->real_output_closure;

#ifdef MIME_DRAFTS
  if ( obj->options &&
	   obj->options->decompose_file_p &&
	   obj->options->decompose_file_output_fn )
	{
	  return obj->options->decompose_file_output_fn 
		(buf, size, obj->options->stream_closure);
	}
  else
#endif /* MIME_DRAFTS */
	{
    if (!closure) {
      MimeObject* lobj = (MimeObject*) relobj;
      closure = lobj->options->stream_closure;
    }
	  return relobj->real_output_fn(buf, size, closure);
	}
}


static int
push_tag(MimeMultipartRelated* relobj, const char* buf, PRInt32 size)
{
	if (size + relobj->curtag_length > relobj->curtag_max) {
		relobj->curtag_max += 2 * size;
		if (relobj->curtag_max < 1024) relobj->curtag_max = 1024;
		if (!relobj->curtag) {
			relobj->curtag = (char*) PR_MALLOC(relobj->curtag_max);
		} else {
			relobj->curtag = (char*) PR_Realloc(relobj->curtag,
												relobj->curtag_max);
		}
		if (!relobj->curtag) return MIME_OUT_OF_MEMORY;
	}
	nsCRT::memcpy(relobj->curtag + relobj->curtag_length, buf, size);
	relobj->curtag_length += size;
	return 0;
}

static int
flush_tag(MimeMultipartRelated* relobj)
{
	int length = relobj->curtag_length;
	char* buf;
	int status;
	
	if (relobj->curtag == NULL || length == 0) return 0;

	status = push_tag(relobj, "", 1);	/* Push on a trailing NULL. */
	if (status < 0) return status;
	buf = relobj->curtag;
	PR_ASSERT(*buf == '<' && buf[length - 1] == '>');
	while (*buf) {
		char c;
		char* absolute;
		char* part_url;
		char* ptr = buf;
		char *ptr2;
		PRBool isquote = PR_FALSE;
		while (*ptr && *ptr != '=') ptr++;
		if (*ptr == '=') {
			ptr++;
			if (*ptr == '"') {
				isquote = PR_TRUE;
				/* Take up the double quote and leading space here as well. */
				/* Safe because there's a '>' at the end */
				do {ptr++;} while (nsCRT::IsAsciiSpace(*ptr));
			}
		}
		status = real_write(relobj, buf, ptr - buf);
		if (status < 0) return status;
		buf = ptr;
		if (!*buf) break;
		if (isquote) 
		{
			ptr = mime_strnchr(buf, '"', length - (buf - relobj->curtag));
		} else {
			for (ptr = buf; *ptr ; ptr++) {
				if (*ptr == '>' || nsCRT::IsAsciiSpace(*ptr)) break;
			}
			PR_ASSERT(*ptr);
		}
		if (!ptr || !*ptr) break;

		while(buf < ptr)
		{
			/* ### mwelch	For each word in the value string, see if
			                the word is a cid: URL. If so, attempt to
							substitute the appropriate mailbox part URL in
							its place. */
			ptr2=buf; /* walk from the left end rightward */
			while((ptr2<ptr) && (!nsCRT::IsAsciiSpace(*ptr2)))
				ptr2++;
			/* Compare the beginning of the word with "cid:". Yuck. */
			if (((ptr2 - buf) > 4) && 
				(buf[0]=='c' && buf[1]=='i' && buf[2]=='d' && buf[3]==':'))
			{
				/* Null terminate the word so we can... */
				c = *ptr2;
				*ptr2 = '\0';
				
				/* Construct a URL out of the word. */
        absolute = MakeAbsoluteURL(relobj->base_url, buf);

				/* See if we have a mailbox part URL
				   corresponding to this cid. */
        part_url = NULL;
        if (absolute)
        {
          part_url = (char *)PL_HashTableLookup(relobj->hash, buf);
				  PR_FREEIF(absolute);
        }
				
				/*If we found a mailbox part URL, write that out instead.*/
				if (part_url)
				{
					status = real_write(relobj, part_url, nsCRT::strlen(part_url));
					if (status < 0) return status;
					buf = ptr2; /* skip over the cid: URL we substituted */
				}
				
				/* Restore the character that we nulled. */
				*ptr2 = c;
			}
      /* rhp - if we get here, we should still check against the hash table! */
      else 
      {
        char holder = *ptr2;
        char *realout;

        *ptr2 = '\0';
    
        /* Construct a URL out of the word. */
				absolute = MakeAbsoluteURL(relobj->base_url, buf);

        /* See if we have a mailbox part URL
				   corresponding to this cid. */
        if (absolute)
				  realout = (char *)PL_HashTableLookup(relobj->hash, absolute);
        else
          realout = (char *)PL_HashTableLookup(relobj->hash, buf);


        *ptr2 = holder;
        PR_FREEIF(absolute);

        if (realout)
        {
          status = real_write(relobj, realout, nsCRT::strlen(realout));
					if (status < 0) return status;
					buf = ptr2; /* skip over the cid: URL we substituted */
        }
      }
      /* rhp - if we get here, we should still check against the hash table! */

			/* Advance to the beginning of the next word, or to
			   the end of the value string. */
			while((ptr2<ptr) && (nsCRT::IsAsciiSpace(*ptr2)))
				ptr2++;

			/* Write whatever original text remains after
			   cid: URL substitution. */
			status = real_write(relobj, buf, ptr2-buf);
			if (status < 0) return status;
			buf = ptr2;
		}
	}
	if (buf && *buf) {
		status = real_write(relobj, buf, nsCRT::strlen(buf));
		if (status < 0) return status;
	}
	relobj->curtag_length = 0;
	return 0;
}



static int
mime_multipart_related_output_fn(char* buf, PRInt32 size, void *stream_closure)
{
	MimeMultipartRelated *relobj = (MimeMultipartRelated *) stream_closure;
	char* ptr;
	PRInt32 delta;
	int status;
	while (size > 0) {
		if (relobj->curtag_length > 0) {
			ptr = mime_strnchr(buf, '>', size);
			if (!ptr) {
				return push_tag(relobj, buf, size);
			}
			delta = ptr - buf + 1;
			status = push_tag(relobj, buf, delta);
			if (status < 0) return status;
			status = flush_tag(relobj);
			if (status < 0) return status;
			buf += delta;
			size -= delta;
		}
		ptr = mime_strnchr(buf, '<', size);
		if (ptr && ptr - buf >= size) ptr = 0;
		if (!ptr) {
			return real_write(relobj, buf, size);
		}
		delta = ptr - buf;
		status = real_write(relobj, buf, delta);
		if (status < 0) return status;
		buf += delta;
		size -= delta;
		PR_ASSERT(relobj->curtag_length == 0);
		status = push_tag(relobj, buf, 1);
		if (status < 0) return status;
		PR_ASSERT(relobj->curtag_length == 1);
		buf++;
		size--;
	}
	return 0;
}


static int
MimeMultipartRelated_parse_eof (MimeObject *obj, PRBool abort_p)
{
	/* OK, all the necessary data has been collected.  We now have to spew out
	   the HTML.  We let it go through all the normal mechanisms (which
	   includes content-encoding handling), and intercept the output data to do
	   translation of the tags.  Whee. */
	MimeMultipartRelated *relobj = (MimeMultipartRelated *) obj;
	int status = 0;
	MimeObject *body;
	char* ct;
	const char* dct;

	status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
	if (status < 0) goto FAIL;

	if (!relobj->headobj) return 0;

	ct = (relobj->buffered_hdrs
		  ? MimeHeaders_get (relobj->buffered_hdrs, HEADER_CONTENT_TYPE,
							 PR_TRUE, PR_FALSE)
		  : 0);
	dct = (((MimeMultipartClass *) obj->clazz)->default_part_type);

	relobj->real_output_fn = obj->options->output_fn;
	relobj->real_output_closure = obj->options->output_closure;

	obj->options->output_fn = mime_multipart_related_output_fn;
	obj->options->output_closure = obj;

	body = mime_create(((ct && *ct) ? ct : (dct ? dct : TEXT_HTML)),
					   relobj->buffered_hdrs, obj->options);
	if (!body) {
		status = MIME_OUT_OF_MEMORY;
		goto FAIL;
	}
	status = ((MimeContainerClass *) obj->clazz)->add_child(obj, body);
	if (status < 0) {
		mime_free(body);
		goto FAIL;
	}

#ifdef MIME_DRAFTS
	if ( obj->options && 
	   obj->options->decompose_file_p &&
	   obj->options->decompose_file_init_fn &&
	   (relobj->file_buffer_spec || relobj->head_buffer))
	{
	  status = obj->options->decompose_file_init_fn ( obj->options->stream_closure,
													  relobj->buffered_hdrs );
	  if (status < 0) return status;
	}
#endif /* MIME_DRAFTS */


	/* Now that we've added this new object to our list of children,
	   start its parser going. */
	status = body->clazz->parse_begin(body);
	if (status < 0) goto FAIL;
	
	if (relobj->head_buffer) 
  {
		/* Read it out of memory. */
		PR_ASSERT(!relobj->file_buffer_spec && !relobj->input_file_stream);

		status = body->clazz->parse_buffer(relobj->head_buffer,
											   relobj->head_buffer_fp,
											   body);
	} 
  else if (relobj->file_buffer_spec) 
  {
		/* Read it off disk. */
		char *buf;
		PRInt32 buf_size = 10 * 1024;  /* 10k; tune this? */

		PR_ASSERT(relobj->head_buffer_size == 0 &&
				  relobj->head_buffer_fp == 0);
		PR_ASSERT(relobj->file_buffer_spec);
		if (!relobj->file_buffer_spec) 
    {
			status = -1;
			goto FAIL;
		}

		buf = (char *) PR_MALLOC(buf_size);
		if (!buf) 
    {
			status = MIME_OUT_OF_MEMORY;
			goto FAIL;
		}

    // First, close the output file to open the input file!
    if (relobj->output_file_stream)
  		relobj->output_file_stream->close();

		relobj->input_file_stream = new nsInputFileStream(*(relobj->file_buffer_spec));
		if (!relobj->input_file_stream) 
    {
			PR_Free(buf);
			status = MIME_UNABLE_TO_OPEN_TMP_FILE;
			goto FAIL;
		}

		while(1) 
    {
			PRInt32 rstatus = relobj->input_file_stream->read(buf, buf_size - 1);
			if (rstatus <= 0) 
      {
				status = rstatus;
				break;
			} 
      else 
      {
				/* It would be really nice to be able to yield here, and let
				   some user events and other input sources get processed.
				   Oh well. */

				status = body->clazz->parse_buffer(buf, rstatus, body);
				if (status < 0) break;
			}
		}
		PR_Free(buf);
	}

  if (status < 0) goto FAIL;

  /* Done parsing. */
  status = body->clazz->parse_eof(body, PR_FALSE);
  if (status < 0) goto FAIL;
  status = body->clazz->parse_end(body, PR_FALSE);
  if (status < 0) goto FAIL;

FAIL:

#ifdef MIME_DRAFTS
  if ( obj->options &&
	   obj->options->decompose_file_p &&
	   obj->options->decompose_file_close_fn &&
	   (relobj->file_buffer_spec || relobj->head_buffer)) {
	status = obj->options->decompose_file_close_fn ( obj->options->stream_closure );
	if (status < 0) return status;
  }
#endif /* MIME_DRAFTS */

  relobj->headobj = NULL;
  obj->options->output_fn = relobj->real_output_fn;
  obj->options->output_closure = relobj->real_output_closure;

  return status;
}	




static int
MimeMultipartRelatedClassInitialize(MimeMultipartRelatedClass *clazz)
{
	MimeObjectClass    *oclass = (MimeObjectClass *) clazz;
	MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;
	PR_ASSERT(!oclass->class_initialized);
	oclass->initialize       = MimeMultipartRelated_initialize;
	oclass->finalize         = MimeMultipartRelated_finalize;
	oclass->parse_eof        = MimeMultipartRelated_parse_eof;
	mclass->output_child_p   = MimeMultipartRelated_output_child_p;
	mclass->parse_child_line = MimeMultipartRelated_parse_child_line;
	return 0;
}
