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

/* mimeext.c --- definition of the MimeExternalBody class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#include "mimeebod.h"
#include "xpgetstr.h"

#define MIME_SUPERCLASS mimeObjectClass
MimeDefClass(MimeExternalBody, MimeExternalBodyClass,
			 mimeExternalBodyClass, &MIME_SUPERCLASS);

extern int MK_MSG_LINK_TO_DOCUMENT;
extern int MK_MSG_DOCUMENT_INFO;

#if defined(XP_MAC) && !defined(MOZILLA_30)
extern MimeObjectClass mimeMultipartAppleDoubleClass;
#endif

static int MimeExternalBody_initialize (MimeObject *);
static void MimeExternalBody_finalize (MimeObject *);
static int MimeExternalBody_parse_line (char *, int32, MimeObject *);
static int MimeExternalBody_parse_eof (MimeObject *, XP_Bool);
static XP_Bool MimeExternalBody_displayable_inline_p (MimeObjectClass *class,
													  MimeHeaders *hdrs);

#if 0
#if defined(DEBUG) && defined(XP_UNIX)
static int MimeExternalBody_debug_print (MimeObject *, FILE *, int32);
#endif
#endif /* 0 */

static int
MimeExternalBodyClassInitialize(MimeExternalBodyClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;

  XP_ASSERT(!oclass->class_initialized);
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
  FREEIF(bod->body);

  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}

static int
MimeExternalBody_parse_line (char *line, int32 length, MimeObject *obj)
{
  MimeExternalBody *bod = (MimeExternalBody *) obj;
  int status = 0;

  XP_ASSERT(line && *line);
  if (!line || !*line) return -1;

  if (!obj->output_p) return 0;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (obj->options &&
	  !obj->options->write_html_p &&
	  obj->options->output_fn)
	return MimeObject_write(obj, line, length, TRUE);


  /* If we already have a `body' then we're done parsing headers, and all
	 subsequent lines get tacked onto the body. */
  if (bod->body)
	{
	  int L = XP_STRLEN(bod->body);
	  char *new_str = XP_REALLOC(bod->body, L + length + 1);
	  if (!new_str) return MK_OUT_OF_MEMORY;
	  bod->body = new_str;
	  XP_MEMCPY(bod->body + L, line, length);
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
	  bod->body = XP_STRDUP("");
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
  else if (!strcasecomp(at, "ftp") || !strcasecomp(at, "anon-ftp"))
	{
	  if (!site || !name)
		return 0;
	  s = (char *) XP_ALLOC(XP_STRLEN(name) + XP_STRLEN(site) +
							(dir  ? XP_STRLEN(dir) : 0) + 20);
	  if (!s) return 0;
	  XP_STRCPY(s, "ftp://");
	  XP_STRCAT(s, site);
	  XP_STRCAT(s, "/");
	  if (dir) XP_STRCAT(s, (dir[0] == '/' ? dir+1 : dir));
	  if (s[XP_STRLEN(s)-1] != '/')
		XP_STRCAT(s, "/");
	  XP_STRCAT(s, name);
	  return s;
	}
  else if (!strcasecomp(at, "local-file") || !strcasecomp(at, "afs"))
	{
	  char *s2;
	  if (!name)
		return 0;

#ifdef XP_UNIX
	  if (!strcasecomp(at, "afs"))   /* only if there is a /afs/ directory */
		{
		  XP_StatStruct st;
		  if (stat("/afs/.", &st))
			return 0;
		}
#else  /* !XP_UNIX */
	  return 0;						/* never, if not Unix. */
#endif /* !XP_UNIX */

	  s = (char *) XP_ALLOC(XP_STRLEN(name)*3 + 20);
	  if (!s) return 0;
	  XP_STRCPY(s, "file:");

	  s2 = NET_Escape(name, URL_PATH);
	  if (s2) XP_STRCAT(s, s2);
	  FREEIF(s2);
	  return s;
	}
  else if (!strcasecomp(at, "mail-server"))
	{
	  char *s2;
	  if (!svr)
		return 0;
	  s = (char *) XP_ALLOC(XP_STRLEN(svr)*4 +
							(subj ? XP_STRLEN(subj)*4 : 0) +
							(body ? XP_STRLEN(body)*4 : 0) + 20);
	  if (!s) return 0;
	  XP_STRCPY(s, "mailto:");

	  s2 = NET_Escape(svr, URL_XALPHAS);
	  if (s2) XP_STRCAT(s, s2);
	  FREEIF(s2);

	  if (subj)
		{
		  s2 = NET_Escape(subj, URL_XALPHAS);
		  XP_STRCAT(s, "?subject=");
		  if (s2) XP_STRCAT(s, s2);
		  FREEIF(s2);
		}
	  if (body)
		{
		  s2 = NET_Escape(body, URL_XALPHAS);
		  XP_STRCAT(s, (subj ? "&body=" : "?body="));
		  if (s2) XP_STRCAT(s, s2);
		  FREEIF(s2);
		}
	  return s;
	}
  else if (!strcasecomp(at, "url"))	    /* RFC 2017 */
	{
	  if (url)
		return XP_STRDUP(url);		   /* it's already quoted and everything */
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
MimeExternalBody_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  int status = 0;
  MimeExternalBody *bod = (MimeExternalBody *) obj;

  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

#if defined(XP_MAC) && !defined(MOZILLA_30)
  if (obj->parent && mime_typep(obj->parent, 
	  (MimeObjectClass*) &mimeMultipartAppleDoubleClass))
	  goto done;
#endif /* defined (XP_MAC) && !defined(MOZILLA_30) */

  if (!abort_p &&
	  obj->output_p &&
	  obj->options &&
	  obj->options->write_html_p)
	{
	  XP_Bool all_headers_p = obj->options->headers == MimeHeadersAll;
	  MimeDisplayOptions newopt = *obj->options;  /* copy it */

	  char *ct = MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE,
								 FALSE, FALSE);
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
	  FREEIF(ct);

	  /* the *internal* content-type */
	  ct = MimeHeaders_get(bod->hdrs, HEADER_CONTENT_TYPE,
						   TRUE, FALSE);

	  h = (char *) XP_ALLOC((at ? XP_STRLEN(at) : 0) +
							(exp ? XP_STRLEN(exp) : 0) +
							(size ? XP_STRLEN(size) : 0) +
							(perm ? XP_STRLEN(perm) : 0) +
							(dir ? XP_STRLEN(dir) : 0) +
							(mode ? XP_STRLEN(mode) : 0) +
							(name ? XP_STRLEN(name) : 0) +
							(site ? XP_STRLEN(site) : 0) +
							(svr ? XP_STRLEN(svr) : 0) +
							(subj ? XP_STRLEN(subj) : 0) +
							(url ? XP_STRLEN(url) : 0) + 100);
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
			if (!XP_IS_SPACE(*in))
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
		  XP_STRCPY(h, STR ": "); \
		  XP_STRCAT(h, VAR); \
		  XP_STRCAT(h, LINEBREAK); \
		  status = MimeHeaders_parse_line(h, XP_STRLEN(h), hdrs); \
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
	  XP_STRCPY(h, LINEBREAK);
	  status = MimeHeaders_parse_line(h, XP_STRLEN(h), hdrs);
	  if (status < 0) goto FAIL;

	  lurl = MimeExternalBody_make_url(ct, at, exp, size, perm, dir, mode,
									   name, url, site, svr, subj, bod->body);
	  if (lurl)
		{
		  lname = XP_STRDUP(XP_GetString(MK_MSG_LINK_TO_DOCUMENT));
		}
	  else
		{
		  lname = XP_STRDUP(XP_GetString(MK_MSG_DOCUMENT_INFO));
		  all_headers_p = TRUE;
		}
		
	  all_headers_p = TRUE;  /* #### just do this all the time? */

	  if (bod->body && all_headers_p)
		{
		  char *s = bod->body;
		  while (XP_IS_SPACE(*s)) s++;
		  if (*s)
			{
			  char *s2;
			  const char *pre = "<P><PRE>";
			  const char *suf = "</PRE>";
			  int32 i;
			  for(i = XP_STRLEN(s)-1; i >= 0 && XP_IS_SPACE(s[i]); i--)
				s[i] = 0;
			  s2 = NET_EscapeHTML(s);
			  if (!s2) goto FAIL;
			  body = (char *) XP_ALLOC(XP_STRLEN(pre) + XP_STRLEN(s2) +
									   XP_STRLEN(suf) + 1);
			  if (!body)
				{
				  XP_FREE(s2);
				  goto FAIL;
				}
			  XP_STRCPY(body, pre);
			  XP_STRCAT(body, s2);
			  XP_STRCAT(body, suf);
			}
		}

	  newopt.fancy_headers_p = TRUE;
	  newopt.headers = (all_headers_p ? MimeHeadersAll : MimeHeadersSome);

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, FALSE);
		if (status < 0) goto FAIL;
	  }

	  status = MimeHeaders_write_attachment_box (hdrs, &newopt, ct, 0,
												 lname, lurl, body);
	  if (status < 0) goto FAIL;

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, FALSE);
		if (status < 0) goto FAIL;
	  }

	FAIL:
	  if (hdrs)
		MimeHeaders_free(hdrs);
	  FREEIF(h);
	  FREEIF(lname);
	  FREEIF(lurl);
	  FREEIF(body);
	  FREEIF(ct);
	  FREEIF(at);
	  FREEIF(exp);
	  FREEIF(size);
	  FREEIF(perm);
	  FREEIF(dir);
	  FREEIF(mode);
	  FREEIF(name);
	  FREEIF(url);
	  FREEIF(site);
	  FREEIF(svr);
	  FREEIF(subj);
	}

#if defined(XP_MAC) && !defined(MOZILLA_30)
done:
#endif /* defined (XP_MAC) && !defined(MOZILLA_30) */

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
MimeExternalBody_debug_print (MimeObject *obj, FILE *stream, int32 depth)
{
  MimeExternalBody *bod = (MimeExternalBody *) obj;
  int i;
  char *ct, *ct2;
  char *addr = mime_part_address(obj);

  if (obj->headers)
	ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE, FALSE, FALSE);
  if (bod->hdrs)
	ct2 = MimeHeaders_get (bod->hdrs, HEADER_CONTENT_TYPE, FALSE, FALSE);

  for (i=0; i < depth; i++)
	fprintf(stream, "  ");
  fprintf(stream,
		  "<%s %s\n"
		  "\tcontent-type: %s\n"
		  "\tcontent-type: %s\n"
		  "\tBody:%s\n\t0x%08X>\n\n",
		  obj->class->class_name,
		  addr ? addr : "???",
		  ct ? ct : "<none>",
		  ct2 ? ct2 : "<none>",
		  bod->body ? bod->body : "<none>",
		  (uint32) obj);
  FREEIF(addr);
  FREEIF(ct);
  FREEIF(ct2);
  return 0;
}
#endif
#endif /* 0 */

static XP_Bool
MimeExternalBody_displayable_inline_p (MimeObjectClass *class,
									   MimeHeaders *hdrs)
{
  char *ct = MimeHeaders_get (hdrs, HEADER_CONTENT_TYPE, FALSE, FALSE);
  char *at = MimeHeaders_get_parameter(ct, "access-type", NULL, NULL);
  XP_Bool inline_p = FALSE;

  if (!at)
	;
  else if (!strcasecomp(at, "ftp") ||
		   !strcasecomp(at, "anon-ftp") ||
		   !strcasecomp(at, "local-file") ||
		   !strcasecomp(at, "mail-server") ||
		   !strcasecomp(at, "url"))
	inline_p = TRUE;
#ifdef XP_UNIX
  else if (!strcasecomp(at, "afs"))   /* only if there is a /afs/ directory */
	{
	  XP_StatStruct st;
	  if (!stat("/afs/.", &st))
		inline_p = TRUE;
	}
#endif /* XP_UNIX */

  FREEIF(ct);
  FREEIF(at);
  return inline_p;
}
