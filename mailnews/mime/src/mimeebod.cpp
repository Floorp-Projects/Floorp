/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "mimeebod.h"
#include "prmem.h"
#include "nsCRT.h"
#include "plstr.h"
#include "prlog.h"
#include "prio.h"
#include "nsFileSpec.h"
#include "nsEscape.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"
#include "mimemoz2.h"

#define MIME_SUPERCLASS mimeObjectClass
MimeDefClass(MimeExternalBody, MimeExternalBodyClass,
			 mimeExternalBodyClass, &MIME_SUPERCLASS);

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
	  int L = nsCRT::strlen(bod->body);
	  char *new_str = (char *)PR_Realloc(bod->body, L + length + 1);
	  if (!new_str) return MIME_OUT_OF_MEMORY;
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
	  if (!bod->hdrs) return MIME_OUT_OF_MEMORY;
	}

  status = MimeHeaders_parse_line(line, length, bod->hdrs);
  if (status < 0) return status;

  /* If this line is blank, we're now done parsing headers, and should
	 create a dummy body to show that.  Gag.
   */
  if (*line == nsCRT::CR || *line == nsCRT::LF)
	{
	  bod->body = nsCRT::strdup("");
	  if (!bod->body) return MIME_OUT_OF_MEMORY;
	}

  return 0;
}


char *
MimeExternalBody_make_url(const char *ct,
						  const char *at, const char *lexp, const char *size,
						  const char *perm, const char *dir, const char *mode,
						  const char *name, const char *url, const char *site,
						  const char *svr, const char *subj, const char *body)
{
  char *s;
  if (!at)
    {
	  return 0;
    }
  else if (!nsCRT::strcasecmp(at, "ftp") || !nsCRT::strcasecmp(at, "anon-ftp"))
	{
	  if (!site || !name)
		return 0;
	  s = (char *) PR_MALLOC(nsCRT::strlen(name) + nsCRT::strlen(site) +
							(dir  ? nsCRT::strlen(dir) : 0) + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "ftp://");
	  PL_strcat(s, site);
	  PL_strcat(s, "/");
	  if (dir) PL_strcat(s, (dir[0] == '/' ? dir+1 : dir));
	  if (s[nsCRT::strlen(s)-1] != '/')
		PL_strcat(s, "/");
	  PL_strcat(s, name);
	  return s;
	}
  else if (!nsCRT::strcasecmp(at, "local-file") || !nsCRT::strcasecmp(at, "afs"))
	{
	  char *s2;
	  if (!name)
		return 0;

#ifdef XP_UNIX
	  if (!nsCRT::strcasecmp(at, "afs"))   /* only if there is a /afs/ directory */
		{
      nsFileSpec    fs("/afs/.");
      
      if  (!fs.Exists())
        return 0;
		}
#else  /* !XP_UNIX */
	  return 0;						/* never, if not Unix. */
#endif /* !XP_UNIX */

	  s = (char *) PR_MALLOC(nsCRT::strlen(name)*3 + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "file:");

	  s2 = nsEscape(name, url_Path);
	  if (s2)
	  {
	      PL_strcat(s, s2);
	      nsCRT::free(s2);
	  }
	  return s;
	}
  else if (!nsCRT::strcasecmp(at, "mail-server"))
	{
	  char *s2;
	  if (!svr)
		return 0;
	  s = (char *) PR_MALLOC(nsCRT::strlen(svr)*4 +
							(subj ? nsCRT::strlen(subj)*4 : 0) +
							(body ? nsCRT::strlen(body)*4 : 0) + 20);
	  if (!s) return 0;
	  PL_strcpy(s, "mailto:");

	  s2 = nsEscape(svr, url_XAlphas);
	  if (s2)
	  {
	      PL_strcat(s, s2);
	      nsCRT::free(s2);
	  }

	  if (subj)
		{
		  s2 = nsEscape(subj, url_XAlphas);
		  PL_strcat(s, "?subject=");
		  if (s2)
		  {
		      PL_strcat(s, s2);
		      nsCRT::free(s2);
		  }
		}
	  if (body)
		{
		  s2 = nsEscape(body, url_XAlphas);
		  PL_strcat(s, (subj ? "&body=" : "?body="));
		  if (s2)
		  {
		      PL_strcat(s, s2);
		      nsCRT::free(s2);
		  }
		}
	  return s;
	}
  else if (!nsCRT::strcasecmp(at, "url"))	    /* RFC 2017 */
	{
	  if (url)
		return nsCRT::strdup(url);		   /* it's already quoted and everything */
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
	  MimeDisplayOptions *newopt = obj->options;  /* copy it */

	  char *ct = MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE,
								 PR_FALSE, PR_FALSE);
	  char *at, *lexp, *size, *perm;
	  char *url, *dir, *mode, *name, *site, *svr, *subj;
	  char *h = 0, *lname = 0, *lurl = 0, *body = 0;
	  MimeHeaders *hdrs = 0;

	  if (!ct) return MIME_OUT_OF_MEMORY;

	  at   = MimeHeaders_get_parameter(ct, "access-type", NULL, NULL);
	  lexp  = MimeHeaders_get_parameter(ct, "expiration", NULL, NULL);
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

	  h = (char *) PR_MALLOC((at ? nsCRT::strlen(at) : 0) +
							(lexp ? nsCRT::strlen(lexp) : 0) +
							(size ? nsCRT::strlen(size) : 0) +
							(perm ? nsCRT::strlen(perm) : 0) +
							(dir ? nsCRT::strlen(dir) : 0) +
							(mode ? nsCRT::strlen(mode) : 0) +
							(name ? nsCRT::strlen(name) : 0) +
							(site ? nsCRT::strlen(site) : 0) +
							(svr ? nsCRT::strlen(svr) : 0) +
							(subj ? nsCRT::strlen(subj) : 0) +
							(url ? nsCRT::strlen(url) : 0) + 100);
	  if (!h)
		{
		  status = MIME_OUT_OF_MEMORY;
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
			if (!nsCRT::IsAsciiSpace(*in))
			  *out++ = *in;
		  *out = 0;
		}

	  hdrs = MimeHeaders_new();
	  if (!hdrs)
		{
		  status = MIME_OUT_OF_MEMORY;
		  goto FAIL;
		}

# define FROB(STR,VAR) \
	  if (VAR) \
		{ \
		  PL_strcpy(h, STR ": "); \
		  PL_strcat(h, VAR); \
		  PL_strcat(h, MSG_LINEBREAK); \
		  status = MimeHeaders_parse_line(h, nsCRT::strlen(h), hdrs); \
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
	  FROB("Expiration",	lexp);
	  FROB("Subject",		subj);
# undef FROB
	  PL_strcpy(h, MSG_LINEBREAK);
	  status = MimeHeaders_parse_line(h, nsCRT::strlen(h), hdrs);
	  if (status < 0) goto FAIL;

	  lurl = MimeExternalBody_make_url(ct, at, lexp, size, perm, dir, mode,
									   name, url, site, svr, subj, bod->body);
	  if (lurl)
		{
		  lname = MimeGetStringByID(MIME_MSG_LINK_TO_DOCUMENT);
		}
	  else
		{
		  lname = MimeGetStringByID(MIME_MSG_DOCUMENT_INFO);
		  all_headers_p = PR_TRUE;
		}
		
	  all_headers_p = PR_TRUE;  /* #### just do this all the time? */

	  if (bod->body && all_headers_p)
		{
		  char *s = bod->body;
		  while (nsCRT::IsAsciiSpace(*s)) s++;
		  if (*s)
			{
			  char *s2;
			  const char *pre = "<P><PRE>";
			  const char *suf = "</PRE>";
			  PRInt32 i;
			  for(i = nsCRT::strlen(s)-1; i >= 0 && nsCRT::IsAsciiSpace(s[i]); i--)
				s[i] = 0;
 			  s2 = nsEscapeHTML(s);
			  if (!s2) goto FAIL;
			  body = (char *) PR_MALLOC(nsCRT::strlen(pre) + nsCRT::strlen(s2) +
									   nsCRT::strlen(suf) + 1);
			  if (!body)
				{
				  nsCRT::free(s2);
				  goto FAIL;
				}
			  PL_strcpy(body, pre);
			  PL_strcat(body, s2);
			  PL_strcat(body, suf);
			}
		}

	  newopt->fancy_headers_p = PR_TRUE;
	  newopt->headers = (all_headers_p ? MimeHeadersAll : MimeHeadersSome);

	FAIL:
	  if (hdrs)
		MimeHeaders_free(hdrs);
	  PR_FREEIF(h);
	  PR_FREEIF(lname);
	  PR_FREEIF(lurl);
	  PR_FREEIF(body);
	  PR_FREEIF(ct);
	  PR_FREEIF(at);
	  PR_FREEIF(lexp);
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
  else if (!nsCRT::strcasecmp(at, "ftp") ||
		   !nsCRT::strcasecmp(at, "anon-ftp") ||
		   !nsCRT::strcasecmp(at, "local-file") ||
		   !nsCRT::strcasecmp(at, "mail-server") ||
		   !nsCRT::strcasecmp(at, "url"))
	inline_p = PR_TRUE;
#ifdef XP_UNIX
  else if (!nsCRT::strcasecmp(at, "afs"))   /* only if there is a /afs/ directory */
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
