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

/*   mimefilt.c --- test harness for libmime.a

   This program reads a message from stdin and writes the output of the MIME
   parser on stdout.

   Parameters can be passed to the parser through the usual URL mechanism:

     mimefilt BASE-URL?headers=all&rot13 < in > out

   Some parameters can't be affected that way, so some additional switches
   may be passed on the command line after the URL:

     -fancy		whether fancy headers should be generated (default)

     -no-fancy	opposite; this uses the headers used in the cases of
				FO_SAVE_AS_TEXT or FO_QUOTE_MESSAGE

     -html		whether we should convert to HTML (like FO_PRESENT);
				this is the default if no ?part= is specified.

     -raw		don't convert to HTML (FO_SAVE_AS);
				this is the default if a ?part= is specified.

     -outline	at the end, print a debugging overview of the MIME structure

   Before any output comes a blurb listing the content-type, charset, and 
   various other info that would have been put in the generated URL struct.
   It's printed to the beginning of the output because otherwise this out-
   of-band data would have been lost.  (So the output of this program is,
   in fact, a raw HTTP response.)
 */

#include "mimemsg.h"
#include "prglobal.h"

#include "key.h"
#include "cert.h"
#include "secrng.h"
#include "secmod.h"
#include "pk11func.h"
#include "nsMimeStringResources.h"

#ifndef XP_UNIX
ERROR!	This is a unix-only file for the "mimefilt" standalone program.
		This does not go into libmime.a.
#endif


static char *
test_file_type (const char *filename, void *stream_closure)
{
  const char *suf = PL_strrchr(filename, '.');
  if (!suf)
	return 0;
  suf++;

  if (!nsCRT::strcasecmp(suf, "txt") ||
	  !nsCRT::strcasecmp(suf, "text"))
	return nsCRT::strdup("text/plain");
  else if (!nsCRT::strcasecmp(suf, "htm") ||
		   !nsCRT::strcasecmp(suf, "html"))
	return nsCRT::strdup("text/html");
  else if (!nsCRT::strcasecmp(suf, "gif"))
	return nsCRT::strdup("image/gif");
  else if (!nsCRT::strcasecmp(suf, "jpg") ||
		   !nsCRT::strcasecmp(suf, "jpeg"))
	return nsCRT::strdup("image/jpeg");
  else if (!nsCRT::strcasecmp(suf, "pjpg") ||
		   !nsCRT::strcasecmp(suf, "pjpeg"))
	return nsCRT::strdup("image/pjpeg");
  else if (!nsCRT::strcasecmp(suf, "xbm"))
	return nsCRT::strdup("image/x-xbitmap");
  else if (!nsCRT::strcasecmp(suf, "xpm"))
	return nsCRT::strdup("image/x-xpixmap");
  else if (!nsCRT::strcasecmp(suf, "xwd"))
	return nsCRT::strdup("image/x-xwindowdump");
  else if (!nsCRT::strcasecmp(suf, "bmp"))
	return nsCRT::strdup("image/x-MS-bmp");
  else if (!nsCRT::strcasecmp(suf, "au"))
	return nsCRT::strdup("audio/basic");
  else if (!nsCRT::strcasecmp(suf, "aif") ||
		   !nsCRT::strcasecmp(suf, "aiff") ||
		   !nsCRT::strcasecmp(suf, "aifc"))
	return nsCRT::strdup("audio/x-aiff");
  else if (!nsCRT::strcasecmp(suf, "ps"))
	return nsCRT::strdup("application/postscript");
  else
	return 0;
}

static char *
test_type_icon(const char *type, void *stream_closure)
{
  if (!nsCRT::strncasecmp(type, "text/", 5))
	return nsCRT::strdup("internal-gopher-text");
  else if (!nsCRT::strncasecmp(type, "image/", 6))
	return nsCRT::strdup("internal-gopher-image");
  else if (!nsCRT::strncasecmp(type, "audio/", 6))
	return nsCRT::strdup("internal-gopher-sound");
  else if (!nsCRT::strncasecmp(type, "video/", 6))
	return nsCRT::strdup("internal-gopher-movie");
  else if (!nsCRT::strncasecmp(type, "application/", 12))
	return nsCRT::strdup("internal-gopher-binary");
  else
	return nsCRT::strdup("internal-gopher-unknown");
}

static int
test_output_fn(char *buf, PRInt32 size, void *closure)
{
  FILE *out = (FILE *) closure;
  if (out)
	return fwrite(buf, sizeof(*buf), size, out);
  else
	return 0;
}

static int
test_output_init_fn (const char *type,
					 const char *charset,
					 const char *name,
					 const char *x_mac_type,
					 const char *x_mac_creator,
					 void *stream_closure)
{
  FILE *out = (FILE *) stream_closure;
  fprintf(out, "CONTENT-TYPE: %s", type);
  if (charset)
	fprintf(out, "; charset=\"%s\"", charset);
  if (name)
	fprintf(out, "; name=\"%s\"", name);
  if (x_mac_type || x_mac_creator)
	fprintf(out, "; x-mac-type=\"%s\"; x-mac-creator=\"%s\"",
			x_mac_type ? x_mac_type : "",
			x_mac_creator ? x_mac_type : "");
  fprintf(out, CRLF CRLF);
  return 0;
}

static void *
test_image_begin(const char *image_url, const char *content_type,
				 void *stream_closure)
{
  return ((void *) nsCRT::strdup(image_url));
}

static void
test_image_end(void *image_closure, int status)
{
  char *url = (char *) image_closure;
  if (url) PR_Free(url);
}

static char *
test_image_make_image_html(void *image_data)
{
  char *url = (char *) image_data;
#if 0
  const char *prefix = "<P><CENTER><IMG SRC=\"";
  const char *suffix = "\"></CENTER><P>";
#else
  const char *prefix = ("<P><CENTER><TABLE BORDER=2 CELLPADDING=20"
						" BGCOLOR=WHITE>"
						"<TR><TD ALIGN=CENTER>"
						"an inlined image would have gone here for<BR>");
  const char *suffix = "</TD></TR></TABLE></CENTER><P>";
#endif
  char *buf;
  buf = (char *) PR_MALLOC (nsCRT::strlen (prefix) + nsCRT::strlen (suffix) +
						   nsCRT::strlen (url) + 20);
  if (!buf) return 0;
  *buf = 0;
  PL_strcat (buf, prefix);
  PL_strcat (buf, url);
  PL_strcat (buf, suffix);
  return buf;
}

static int test_image_write_buffer(char *buf, PRInt32 size, void *image_closure)
{
  return 0;
}

static char *
test_passwd_prompt (PK11SlotInfo *slot, void *wincx)
{
  char buf[2048], *s;
  fprintf(stdout, "#### Password required: ");
  s = fgets(buf, sizeof(buf)-1, stdin);
  if (!s) return s;
  if (s[nsCRT::strlen(s)-1] == '\r' ||
	  s[nsCRT::strlen(s)-1] == '\n')
	s[nsCRT::strlen(s)-1] = '\0';
  return s;
}


int
test(FILE *in, FILE *out,
	 const char *url,
	 PRBool fancy_headers_p,
	 PRBool html_p,
	 PRBool outline_p,
	 PRBool dexlate_p,
	 PRBool variable_width_plaintext_p)
{
  int status = 0;
  MimeObject *obj = 0;
  MimeDisplayOptions *opt = new MimeDisplayOptions;
//  memset(opt, 0, sizeof(*opt));

  if (dexlate_p) html_p = PR_FALSE;

  opt->fancy_headers_p = fancy_headers_p;
  opt->headers = MimeHeadersSome;
  opt->rot13_p = PR_FALSE;

  status = mime_parse_url_options(url, opt);
  if (status < 0)
	{
	  PR_Free(opt);
	  return MIME_OUT_OF_MEMORY;
	}

  opt->url					= url;
  opt->write_html_p			= html_p;
  opt->dexlate_p			= dexlate_p;
  opt->output_init_fn		= test_output_init_fn;
  opt->output_fn			= test_output_fn;
  opt->charset_conversion_fn= 0;
  opt->rfc1522_conversion_p = PR_FALSE;
  opt->file_type_fn			= test_file_type;
  opt->stream_closure		= out;

  opt->image_begin			= test_image_begin;
  opt->image_end			= test_image_end;
  opt->make_image_html		= test_image_make_image_html;
  opt->image_write_buffer	= test_image_write_buffer;

  opt->variable_width_plaintext_p = variable_width_plaintext_p;

  obj = mime_new ((MimeObjectClass *)&mimeMessageClass,
				  (MimeHeaders *) NULL,
				  MESSAGE_RFC822);
  if (!obj)
	{
	  PR_Free(opt);
	  return MIME_OUT_OF_MEMORY;
	}
  obj->options = opt;

  status = obj->class->initialize(obj);
  if (status >= 0)
	status = obj->class->parse_begin(obj);
  if (status < 0)
	{
	  PR_Free(opt);
	  PR_Free(obj);
	  return MIME_OUT_OF_MEMORY;
	}

  while (1)
	{
	  char buf[255];
	  int size = fread(buf, sizeof(*buf), sizeof(buf), stdin);
	  if (size <= 0) break;
	  status = obj->class->parse_buffer(buf, size, obj);
	  if (status < 0)
		{
		  mime_free(obj);
		  PR_Free(opt);
		  return status;
		}
	}

  status = obj->class->parse_eof(obj, PR_FALSE);
  if (status >= 0)
	status = obj->class->parse_end(obj, PR_FALSE);
  if (status < 0)
	{
	  mime_free(obj);
	  PR_Free(opt);
	  return status;
	}

  if (outline_p)
	{
	  fprintf(out, "\n\n"
		  "###############################################################\n");
	  obj->class->debug_print(obj, stderr, 0);
	  fprintf(out,
		  "###############################################################\n");
	}

  mime_free (obj);
  PR_Free(opt);
  return 0;
}


static char *
test_cdb_name_cb (void *arg, int vers)
{
  static char f[1024];
  if (vers <= 4)
	sprintf(f, "%s/.netscape/cert.db", getenv("HOME"));
  else
	sprintf(f, "%s/.netscape/cert%d.db", getenv("HOME"), vers);
  return f;
}

static char *
test_kdb_name_cb (void *arg, int vers)
{
  static char f[1024];
  if (vers <= 2)
	sprintf(f, "%s/.netscape/key.db", getenv("HOME"));
  else
	sprintf(f, "%s/.netscape/key%d.db", getenv("HOME"), vers);
  return f;
}

extern void SEC_Init(void);

int
main (int argc, char **argv)
{
  PRInt32 i = 1;
  char *url = "";
  PRBool fancy_p = PR_TRUE;
  PRBool html_p = PR_TRUE;
  PRBool outline_p = PR_FALSE;
  PRBool dexlate_p = PR_FALSE;
  char filename[1000];
  CERTCertDBHandle *cdb_handle;
  SECKEYKeyDBHandle *kdb_handle;

  PR_Init("mimefilt", 24, 1, 0);

  cdb_handle = (CERTCertDBHandle *)  malloc(sizeof(*cdb_handle));
  memset(cdb_handle, 0, sizeof(*cdb_handle));

  if (SECSuccess != CERT_OpenCertDB(cdb_handle, PR_FALSE, test_cdb_name_cb, NULL))
	CERT_OpenVolatileCertDB(cdb_handle);
  CERT_SetDefaultCertDB(cdb_handle);

  RNG_RNGInit();

  kdb_handle = SECKEY_OpenKeyDB(PR_FALSE, test_kdb_name_cb, NULL);
  SECKEY_SetDefaultKeyDB(kdb_handle);

  PK11_SetPasswordFunc(test_passwd_prompt);

  sprintf(filename, "%s/.netscape/secmodule.db", getenv("HOME"));
  SECMOD_init(filename);

  SEC_Init();


  if (i < argc)
	{
	  if (argv[i][0] == '-')
		url = nsCRT::strdup("");
	  else
		url = argv[i++];
	}

  if (url &&
	  (PL_strstr(url, "?part=") ||
	   PL_strstr(url, "&part=")))
	html_p = PR_FALSE;

  while (i < argc)
	{
	  if (!nsCRT::strcmp(argv[i], "-fancy"))
		fancy_p = PR_TRUE;
	  else if (!nsCRT::strcmp(argv[i], "-no-fancy"))
		fancy_p = PR_FALSE;
	  else if (!nsCRT::strcmp(argv[i], "-html"))
		html_p = PR_TRUE;
	  else if (!nsCRT::strcmp(argv[i], "-raw"))
		html_p = PR_FALSE;
	  else if (!nsCRT::strcmp(argv[i], "-outline"))
		outline_p = PR_TRUE;
	  else if (!nsCRT::strcmp(argv[i], "-dexlate"))
		dexlate_p = PR_TRUE;
	  else
		{
		  fprintf(stderr,
		  "usage: %s [ URL [ -fancy | -no-fancy | -html | -raw | -outline | -dexlate ]]\n"
				  "     < message/rfc822 > output\n",
				  (PL_strrchr(argv[0], '/') ?
				   PL_strrchr(argv[0], '/') + 1 :
				   argv[0]));
		  i = 1;
		  goto FAIL;
		}
	  i++;
	}

  i = test(stdin, stdout, url, fancy_p, html_p, outline_p, dexlate_p, PR_TRUE);
  fprintf(stdout, "\n");
  fflush(stdout);

 FAIL:

  CERT_ClosePermCertDB(cdb_handle);
  SECKEY_CloseKeyDB(kdb_handle);

  exit(i);
}
