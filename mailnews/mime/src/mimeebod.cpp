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
#include "mimeebod.h"
#include "prmem.h"
#include "nsCRT.h"
#include "plstr.h"
#include "prio.h"
#include "nsFileSpec.h"
#include "nsEscape.h"
#include "msgCore.h"
#include "nsMimeTransition.h"

#define MIME_SUPERCLASS mimeObjectClass
MimeDefClass(MimeExternalBody, MimeExternalBodyClass,
			 mimeExternalBodyClass, &MIME_SUPERCLASS);

extern "C" int MK_MSG_LINK_TO_DOCUMENT;
extern "C" int MK_MSG_DOCUMENT_INFO;

#ifdef XP_MAC
extern MimeObjectClass mimeMultipartAppleDoubleClass;
#endif

static int MimeExternalBody_initialize (MimeObject *);
static void MimeExternalBody_finalize (MimeObject *);
static int MimeExternalBody_parse_line (char *, PRInt32, MimeObject *);
static int MimeExternalBody_parse_eof (MimeObject *, PRBool);
static PRBool MimeExternalBody_displayable_inline_p (MimeObjectClass *clazz,
													  MimeHeaders *hdrs);

#if 0
#if defined(DEBUG) && defined(XP_UNIX)
static int MimeExternalBody_debug_print (MimeObject *, PRFileDesc *, PRInt32);
#endif
#endif /* 0 */

static int
MimeExternalBodyClassInitialize(MimeExternalBodyClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize  = MimeExternalBody_initialize;
  oclass->finalize    = MimeExternalBody_finalize;
  oclass->parse_line  = MimeExternalBody_parse_line;
  oclass->parse_eof  = MimeExternalBody_parse_eof;
  oclass->displayable_inline_p = MimeExternalBody_displayable_inline_p;

#if 0
#if defined(DEBUG) && defined(XP_UNIX)
  oclass->debug_print = MimeExternalBody_debug_print;
#endif
#endif /* 0 */

  return 0;
}


static int
MimeExternalBody_initialize (MimeObject *object)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeExternalBody_finalize (MimeObject *object)
{
  MimeExternalBody *bod = (MimeExternalBody *) object;
  if (bod->hdrs)
	{
	  MimeHeaders_free(bod->hdrs);
	  bod->hdrs = 0;
	}
  PR_FREEIF(bod->body);

  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}

static int
MimeExternalBody_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  MimeExternalBody *bod = (MimeExternalBody *) obj;
  int status = 0;

  PR_ASSERT(line && *line);
  if (!line || !*line) return -1;

  if (!obj->output_p) return 0;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (obj->options &&
	  !obj->options->write_html_p &&
	  obj->options->output_fn)
	return MimeObject_write(obj, line, length, PR_TRUE);


  /* If we already have a `body' then we're done parsing headers, and all
	 subsequent lines get tacked onto the body. */
  if (bod->body)
	{
	  int L = PL_strlen(bod->body);
	  char *new_str = (char *)PR_Realloc(bod->body, L + length + 1);
	  if (!new_str) return MK_OUT_OF_MEMORY;
	  bod->body = new_str;
	  nsCRT::memcpy(bod->body + L, line, length);
	  bod->body[L + length] = 0;
	  return 0;
	}

  /* Otherwise we don't yet have a body, which means we're not done parsing
	 our headers.
   */
  if (!bod->hdrs)
	{
	  bod->hdrs = MimeHeaders_new();
	  if (!bod->hdrs) return MK_OUT_OF_MEMORY;
	}

  status = MimeHeaders_parse_line(line, length, bod->hdrs);
  if (status < 0) return status;

  /* If this line is blank, we're now done parsing headers, and should
	 create a dummy body to show that.  Gag.
   */
  if (*line == CR || *line == LF)
	{
	  bod->body = PL_strdup("");
	  if (!bod->body) return MK_OUT_OF_MEMORY;
	}

  return 0;
}


char *
MimeExternalBody_make_url(const char *ct,
						  const char *at, const char *exp, const char *size,
						  const char *perm, const char *dir, const char *mode,
						  const char *name, const char *url, const char *site,
						  const char *svr, const char *subj, const char *body)
{
  char *s;
  if (!at)
    {
	  return 0;
    }
  else if (!PL_strcasecmp(at, "ftp") || !PL_strcasecmp(at, "anon-ftp"))
	{
	  if (!site || !name)
		return 0;
	  s = (char *) PR_MALLOC(PL_strlen(name) + PL_strlen(site) +
							(dir  ? PL_strlen(dir) : 0) + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "ftp://");
	  PL_strcat(s, site);
	  PL_strcat(s, "/");
	  if (dir) PL_strcat(s, (dir[0] == '/' ? dir+1 : dir));
	  if (s[PL_strlen(s)-1] != '/')
		PL_strcat(s, "/");
	  PL_strcat(s, name);
	  return s;
	}
  else if (!PL_strcasecmp(at, "local-file") || !PL_strcasecmp(at, "afs"))
	{
	  char *s2;
	  if (!name)
		return 0;

#ifdef XP_UNIX
	  if (!PL_strcasecmp(at, "afs"))   /* only if there is a /afs/ directory */
		{
      nsFileSpec    fs("/afs/.");
      
      if  (!fs.Exists())
        return 0;
		}
#else  /* !XP_UNIX */
	  return 0;						/* never, if not Unix. */
#endif /* !XP_UNIX */

	  s = (char *) PR_MALLOC(PL_strlen(name)*3 + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "file:");

	  s2 = nsEscape(name, url_Path);
	  if (s2) PL_strcat(s, s2);
	  PR_FREEIF(s2);
	  return s;
	}
  else if (!PL_strcasecmp(at, "mail-server"))
	{
	  char *s2;
	  if (!svr)
		return 0;
	  s = (char *) PR_MALLOC(PL_strlen(svr)*4 +
							(subj ? PL_strlen(subj)*4 : 0) +
							(body ? PL_strlen(body)*4 : 0) + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "mailto:");

	  s2 = nsEscape(svr, url_XAlphas);
	  if (s2) PL_strcat(s, s2);
	  PR_FREEIF(s2);

	  if (subj)
		{
		  s2 = nsEscape(subj, url_XAlphas);
		  PL_strcat(s, "?subject=");
		  if (s2) PL_strcat(s, s2);
		  PR_FREEIF(s2);
		}
	  if (body)
		{
		  s2 = nsEscape(body, url_XAlphas);
		  PL_strcat(s, (subj ? "&body=" : "?body="));
		  if (s2) PL_strcat(s, s2);
		  PR_FREEIF(s2);
		}
	  return s;
	}
  else if (!PL_strcasecmp(at, "url"))	    /* RFC 2017 */
	{
	  if (url)
		return PL_strdup(url);		   /* it's already quoted and everything */
	  else
		return 0;
	}
  else
	return 0;
}

#ifdef XP_MAC
#ifdef DEBUG
#pragma global_optimizer on
#pragma optimization_level 1
#endif /* DEBUG */
#endif /* XP_MAC */

static int
MimeExternalBody_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status = 0;
  MimeExternalBody *bod = (MimeExternalBody *) obj;

  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

#ifdef XP_MAC
  if (obj->parent && mime_typep(obj->parent, 
	  (MimeObjectClass*) &mimeMultipartAppleDoubleClass))
	  goto done;
#endif /* XP_MAC */

  if (!abort_p &&
	  obj->output_p &&
	  obj->options &&
	  obj->options->write_html_p)
	{
	  PRBool all_headers_p = obj->options->headers == MimeHeadersAll;
	  MimeDisplayOptions newopt = *obj->options;  /* copy it */

	  char *ct = MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE,
								 PR_FALSE, PR_FALSE);
	  char *at, *exp, *size, *perm;
	  char *url, *dir, *mode, *name, *site, *svr, *subj;
	  char *h = 0, *lname = 0, *lurl = 0, *body = 0;
	  MimeHeaders *hdrs = 0;

	  if (!ct) return MK_OUT_OF_MEMORY;

	  at   = MimeHeaders_get_parameter(ct, "access-type", NULL, NULL);
	  exp  = MimeHeaders_get_parameter(ct, "expiration", NULL, NULL);
	  size = MimeHeaders_get_parameter(ct, "size", NULL, NULL);
	  perm = MimeHeaders_get_parameter(ct, "permission", NULL, NULL);
	  dir  = MimeHeaders_get_parameter(ct, "directory", NULL, NULL);
	  mode = MimeHeaders_get_parameter(ct, "mode", NULL, NULL);
	  name = MimeHeaders_get_parameter(ct, "name", NULL, NULL);
	  site = MimeHeaders_get_parameter(ct, "site", NULL, NULL);
	  svr  = MimeHeaders_get_parameter(ct, "server", NULL, NULL);
	  subj = MimeHeaders_get_parameter(ct, "subject", NULL, NULL);
	  url  = MimeHeaders_get_parameter(ct, "url", NULL, NULL);
	  PR_FREEIF(ct);

	  /* the *internal* content-type */
	  ct = MimeHeaders_get(bod->hdrs, HEADER_CONTENT_TYPE,
						   PR_TRUE, PR_FALSE);

	  h = (char *) PR_MALLOC((at ? PL_strlen(at) : 0) +
							(exp ? PL_strlen(exp) : 0) +
							(size ? PL_strlen(size) : 0) +
							(perm ? PL_strlen(perm) : 0) +
							(dir ? PL_strlen(dir) : 0) +
							(mode ? PL_strlen(mode) : 0) +
							(name ? PL_strlen(name) : 0) +
							(site ? PL_strlen(site) : 0) +
							(svr ? PL_strlen(svr) : 0) +
							(subj ? PL_strlen(subj) : 0) +
							(url ? PL_strlen(url) : 0) + 100);
	  if (!h)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}

	  /* If there's a URL parameter, remove all whitespace from it.
		 (The URL parameter to one of these headers is stored with
		 lines broken every 40 characters or less; it's assumed that
		 all significant whitespace was URL-hex-encoded, and all the
		 rest of it was inserted just to keep the lines short.)
	   */
	  if (url)
		{
		  char *in, *out;
		  for (in = url, out = url; *in; in++)
			if (!IS_SPACE(*in))
			  *out++ = *in;
		  *out = 0;
		}

	  hdrs = MimeHeaders_new();
	  if (!hdrs)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}

# define FROB(STR,VAR) \
	  if (VAR) \
		{ \
		  PL_strcpy(h, STR ": "); \
		  PL_strcat(h, VAR); \
		  PL_strcat(h, LINEBREAK); \
		  status = MimeHeaders_parse_line(h, PL_strlen(h), hdrs); \
		  if (status < 0) goto FAIL; \
		}
	  FROB("Access-Type",	at);
	  FROB("URL",			url);
	  FROB("Site",			site);
	  FROB("Server",		svr);
	  FROB("Directory",		dir);
	  FROB("Name",			name);
      FROB("Type",			ct);
	  FROB("Size",			size);
	  FROB("Mode",			mode);
	  FROB("Permission",	perm);
	  FROB("Expiration",	exp);
	  FROB("Subject",		subj);
# undef FROB
	  PL_strcpy(h, LINEBREAK);
	  status = MimeHeaders_parse_line(h, PL_strlen(h), hdrs);
	  if (status < 0) goto FAIL;

	  lurl = MimeExternalBody_make_url(ct, at, exp, size, perm, dir, mode,
									   name, url, site, svr, subj, bod->body);
	  if (lurl)
		{
		  lname = PL_strdup(XP_GetString(MK_MSG_LINK_TO_DOCUMENT));
		}
	  else
		{
		  lname = PL_strdup(XP_GetString(MK_MSG_DOCUMENT_INFO));
		  all_headers_p = PR_TRUE;
		}
		
	  all_headers_p = PR_TRUE;  /* #### just do this all the time? */

	  if (bod->body && all_headers_p)
		{
		  char *s = bod->body;
		  while (IS_SPACE(*s)) s++;
		  if (*s)
			{
			  char *s2;
			  const char *pre = "<P><PRE>";
			  const char *suf = "</PRE>";
			  PRInt32 i;
			  for(i = PL_strlen(s)-1; i >= 0 && IS_SPACE(s[i]); i--)
				s[i] = 0;
 			  s2 = nsEscapeHTML(s);
			  if (!s2) goto FAIL;
			  body = (char *) PR_MALLOC(PL_strlen(pre) + PL_strlen(s2) +
									   PL_strlen(suf) + 1);
			  if (!body)
				{
				  PR_Free(s2);
				  goto FAIL;
				}
			  PL_strcpy(body, pre);
			  PL_strcat(body, s2);
			  PL_strcat(body, suf);
			}
		}

	  newopt.fancy_headers_p = PR_TRUE;
	  newopt.headers = (all_headers_p ? MimeHeadersAll : MimeHeadersSome);

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, PR_FALSE);
		if (status < 0) goto FAIL;
	  }

	  status = MimeHeaders_write_attachment_box (hdrs, &newopt, ct, 0,
												 lname, lurl, body);
	  if (status < 0) goto FAIL;

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, PR_FALSE);
		if (status < 0) goto FAIL;
	  }

	FAIL:
	  if (hdrs)
		MimeHeaders_free(hdrs);
	  PR_FREEIF(h);
	  PR_FREEIF(lname);
	  PR_FREEIF(lurl);
	  PR_FREEIF(body);
	  PR_FREEIF(ct);
	  PR_FREEIF(at);
	  PR_FREEIF(exp);
	  PR_FREEIF(size);
	  PR_FREEIF(perm);
	  PR_FREEIF(dir);
	  PR_FREEIF(mode);
	  PR_FREEIF(name);
	  PR_FREEIF(url);
	  PR_FREEIF(site);
	  PR_FREEIF(svr);
	  PR_FREEIF(subj);
	}

#ifdef XP_MAC
done:
#endif /* XP_MAC */

  return status;
}

#ifdef XP_MAC
#ifdef DEBUG
#pragma global_optimizer reset
#endif /* DEBUG */
#endif /* XP_MAC */

#if 0
#if defined(DEBUG) && defined(XP_UNIX)
static int
MimeExternalBody_debug_print (MimeObject *obj, PRFileDesc *stream, PRInt32 depth)
{
  MimeExternalBody *bod = (MimeExternalBody *) obj;
  int i;
  char *ct, *ct2;
  char *addr = mime_part_address(obj);

  if (obj->headers)
	ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  if (bod->hdrs)
	ct2 = MimeHeaders_get (bod->hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);

  for (i=0; i < depth; i++)
	PR_Write(stream, "  ", 2);
/***
  fprintf(stream,
		  "<%s %s\n"
		  "\tcontent-type: %s\n"
		  "\tcontent-type: %s\n"
		  "\tBody:%s\n\t0x%08X>\n\n",
		  obj->clazz->class_name,
		  addr ? addr : "???",
		  ct ? ct : "<none>",
		  ct2 ? ct2 : "<none>",
		  bod->body ? bod->body : "<none>",
		  (PRUint32) obj);
***/
  PR_FREEIF(addr);
  PR_FREEIF(ct);
  PR_FREEIF(ct2);
  return 0;
}
#endif
#endif /* 0 */

static PRBool
MimeExternalBody_displayable_inline_p (MimeObjectClass *clazz,
									   MimeHeaders *hdrs)
{
  char *ct = MimeHeaders_get (hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  char *at = MimeHeaders_get_parameter(ct, "access-type", NULL, NULL);
  PRBool inline_p = PR_FALSE;

  if (!at)
	;
  else if (!PL_strcasecmp(at, "ftp") ||
		   !PL_strcasecmp(at, "anon-ftp") ||
		   !PL_strcasecmp(at, "local-file") ||
		   !PL_strcasecmp(at, "mail-server") ||
		   !PL_strcasecmp(at, "url"))
	inline_p = PR_TRUE;
#ifdef XP_UNIX
  else if (!PL_strcasecmp(at, "afs"))   /* only if there is a /afs/ directory */
	{
    nsFileSpec    fs("/afs/.");
    if  (!fs.Exists())
      return 0;

    inline_p = PR_TRUE;
	}
#endif /* XP_UNIX */

  PR_FREEIF(ct);
  PR_FREEIF(at);
  return inline_p;
}
