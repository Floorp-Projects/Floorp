/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "msgCore.h"
#include "mimei.h"
#include "prmem.h"
#include "prlog.h"
#include "plstr.h"
#include "mimebuf.h"
#include "mimemoz2.h"
#include "nsIMimeEmitter.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsMsgMessageFlags.h"
#include "nsMimeAddress.h"
#include "comi18n.h"
#include "nsMailHeaders.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"
#include "mimemoz2.h"
#include "nsMsgI18N.h"
#include "mimehdrs.h"
#include "nsIMIMEHeaderParam.h" 
#include "nsNetCID.h"

// Forward declares...
PRInt32 MimeHeaders_build_heads_list(MimeHeaders *hdrs);

static void
MimeHeaders_convert_header_value(MimeDisplayOptions *opt, nsAFlatCString &value)
{
  char        *converted;

  if (value.IsEmpty())
    return;

  if (opt && opt->rfc1522_conversion_p)
  {
    converted = MIME_DecodeMimeHeader(value.get(), opt->default_charset,
                                      opt->override_charset, PR_TRUE);

    if (converted)
    {
      value.Adopt(converted);
    }
  }
  else
  {
    // This behavior, though highly unusual, was carefully preserved
    // from the previous implementation.  It may be that this is dead
    // code, in which case opt->rfc1522_conversion_p is no longer
    // needed.
    value.Truncate();
  }
}


MimeHeaders *
MimeHeaders_new (void)
{
  MimeHeaders *hdrs = (MimeHeaders *) PR_MALLOC(sizeof(MimeHeaders));
  if (!hdrs) return 0;

  memset(hdrs, 0, sizeof(*hdrs));
  hdrs->done_p = PR_FALSE;

  return hdrs;
}

void
MimeHeaders_free (MimeHeaders *hdrs)
{
  if (!hdrs) return;
  PR_FREEIF(hdrs->all_headers);
  PR_FREEIF(hdrs->heads);
  PR_FREEIF(hdrs->obuffer);
  PR_FREEIF(hdrs->munged_subject);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

# ifdef DEBUG__
  {
	int i, size = sizeof(*hdrs);
	PRUint32 *array = (PRUint32*) hdrs;
	for (i = 0; i < (size / sizeof(*array)); i++)
	  array[i] = (PRUint32) 0xDEADBEEF;
  }
# endif /* DEBUG */

  PR_Free(hdrs);
}

int
MimeHeaders_parse_line (const char *buffer, PRInt32 size, MimeHeaders *hdrs)
{
  int status = 0;
  int desired_size;

  NS_ASSERTION(hdrs, "1.22 <rhp@netscape.com> 22 Aug 1999 08:48");
  if (!hdrs) return -1;

  /* Don't try and feed me more data after having fed me a blank line... */
  NS_ASSERTION(!hdrs->done_p, "1.22 <rhp@netscape.com> 22 Aug 1999 08:48");
  if (hdrs->done_p) return -1;

  if (!buffer || size == 0 || *buffer == nsCRT::CR || *buffer == nsCRT::LF)
	{
	  /* If this is a blank line, we're done.
	   */
	  hdrs->done_p = PR_TRUE;
	  return MimeHeaders_build_heads_list(hdrs);
	}

  /* Tack this data on to the end of our copy.
   */
  desired_size = hdrs->all_headers_fp + size + 1;
  if (desired_size >= hdrs->all_headers_size)
	{
	  status = mime_GrowBuffer (desired_size, sizeof(char), 255,
							   &hdrs->all_headers, &hdrs->all_headers_size);
	  if (status < 0) return status;
	}
  memcpy(hdrs->all_headers+hdrs->all_headers_fp, buffer, size);
  hdrs->all_headers_fp += size;

  return 0;
}

MimeHeaders *
MimeHeaders_copy (MimeHeaders *hdrs)
{
  MimeHeaders *hdrs2;
  if (!hdrs) return 0;

  hdrs2 = (MimeHeaders *) PR_MALLOC(sizeof(*hdrs));
  if (!hdrs2) return 0;
  memset(hdrs2, 0, sizeof(*hdrs2));

  if (hdrs->all_headers)
	{
	  hdrs2->all_headers = (char *) PR_MALLOC(hdrs->all_headers_fp);
	  if (!hdrs2->all_headers)
		{
		  PR_Free(hdrs2);
		  return 0;
		}
	  memcpy(hdrs2->all_headers, hdrs->all_headers, hdrs->all_headers_fp);

	  hdrs2->all_headers_fp   = hdrs->all_headers_fp;
	  hdrs2->all_headers_size = hdrs->all_headers_fp;
	}

  hdrs2->done_p = hdrs->done_p;

  if (hdrs->heads)
	{
	  int i;
	  hdrs2->heads = (char **) PR_MALLOC(hdrs->heads_size
										* sizeof(*hdrs->heads));
	  if (!hdrs2->heads)
		{
		  PR_FREEIF(hdrs2->all_headers);
		  PR_Free(hdrs2);
		  return 0;
		}
	  hdrs2->heads_size = hdrs->heads_size;
	  for (i = 0; i < hdrs->heads_size; i++)
		{
		  hdrs2->heads[i] = (hdrs2->all_headers +
							 (hdrs->heads[i] - hdrs->all_headers));
		}
	}
  return hdrs2;
}

int
MimeHeaders_build_heads_list(MimeHeaders *hdrs)
{
  char *s;
  char *end;
  int i;
  NS_ASSERTION(hdrs, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!hdrs) return -1;

  NS_ASSERTION(hdrs->done_p && !hdrs->heads, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!hdrs->done_p || hdrs->heads)
	return -1;

  if (hdrs->all_headers_fp == 0)
	{
	  /* Must not have been any headers (we got the blank line right away.) */
	  PR_FREEIF (hdrs->all_headers);
	  hdrs->all_headers_size = 0;
	  return 0;
	}

  /* At this point, we might as well realloc all_headers back down to the
	 minimum size it must be (it could be up to 1k bigger.)  But don't
	 bother if we're only off by a tiny bit. */
  NS_ASSERTION(hdrs->all_headers_fp <= hdrs->all_headers_size, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (hdrs->all_headers_fp + 60 <= hdrs->all_headers_size)
	{
	  char *ls = (char *)PR_Realloc(hdrs->all_headers, hdrs->all_headers_fp);
	  if (ls) /* can this ever fail?  we're making it smaller... */
		{
		  hdrs->all_headers = ls;  /* in case it got relocated */
		  hdrs->all_headers_size = hdrs->all_headers_fp;
		}
	}


  /* First go through and count up the number of headers in the block.
   */
  end = hdrs->all_headers + hdrs->all_headers_fp;
  for (s = hdrs->all_headers; s <= end-1; s++)
	{
	  if (s <= (end-1) && s[0] == nsCRT::CR && s[1] == nsCRT::LF) /* CRLF -> LF */
		s++;

	  if ((s[0] == nsCRT::CR || s[0] == nsCRT::LF) &&			/* we're at a newline, and */
		  (s >= (end-1) ||						/* we're at EOF, or */
		   !(s[1] == ' ' || s[1] == '\t')))		/* next char is nonwhite */
		hdrs->heads_size++;
	}
	  
  /* Now allocate storage for the pointers to each of those headers.
   */
  hdrs->heads = (char **) PR_MALLOC((hdrs->heads_size + 1) * sizeof(char *));
  if (!hdrs->heads)
	return MIME_OUT_OF_MEMORY;
  memset(hdrs->heads, 0, (hdrs->heads_size + 1) * sizeof(char *));


  /* Now make another pass through the headers, and this time, record the
	 starting position of each header.
   */

  i = 0;
  hdrs->heads[i++] = hdrs->all_headers;
  s = hdrs->all_headers;

  while (s <= end)
	{
	SEARCH_NEWLINE:
	  while (s <= end-1 && *s != nsCRT::CR && *s != nsCRT::LF)
		s++;

	  if (s+1 >= end)
		break;

	  /* If "\r\n " or "\r\n\t" is next, that doesn't terminate the header. */
	  else if (s+2 < end &&
			   (s[0] == nsCRT::CR  && s[1] == nsCRT::LF) &&
			   (s[2] == ' ' || s[2] == '\t'))
		{
		  s += 3;
		  goto SEARCH_NEWLINE;
		}
	  /* If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
		 the header either. */
	  else if ((s[0] == nsCRT::CR  || s[0] == nsCRT::LF) &&
			   (s[1] == ' ' || s[1] == '\t'))
		{
		  s += 2;
		  goto SEARCH_NEWLINE;
		}

	  /* At this point, `s' points before a header-terminating newline.
		 Move past that newline, and store that new position in `heads'.
	   */
	  if (*s == nsCRT::CR) s++;
	  if (*s == nsCRT::LF) s++;

	  if (s < end)
		{
		  NS_ASSERTION(! (i > hdrs->heads_size), "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
		  if (i > hdrs->heads_size) return -1;
		  hdrs->heads[i++] = s;
		}
	}

  return 0;
}

char *
MimeHeaders_get (MimeHeaders *hdrs, const char *header_name,
				 PRBool strip_p, PRBool all_p)
{
  int i;
  int name_length;
  char *result = 0;

/*  PR_ASSERT(hdrs); cause delete message problem in WinFE */
  if (!hdrs) return 0;
  NS_ASSERTION(header_name, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!header_name) return 0;

  /* Specifying strip_p and all_p at the same time doesn't make sense... */
  NS_ASSERTION(!(strip_p && all_p), "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

  /* One shouldn't be trying to read headers when one hasn't finished
	 parsing them yet... but this can happen if the message ended
	 prematurely, and has no body at all (as opposed to a null body,
	 which is more normal.)   So, if we try to read from the headers,
	 let's assume that the headers are now finished.  If they aren't
	 in fact finished, then a later attempt to write to them will assert.
   */
  if (!hdrs->done_p)
	{
	  int status;
	  hdrs->done_p = PR_TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  if (!hdrs->heads)	  /* Must not have been any headers. */
	{
	  NS_ASSERTION(hdrs->all_headers_fp == 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
	  return 0;
	}

  name_length = strlen(header_name);

  for (i = 0; i < hdrs->heads_size; i++)
	{
	  char *head = hdrs->heads[i];
	  char *end = (i == hdrs->heads_size-1
				   ? hdrs->all_headers + hdrs->all_headers_fp
				   : hdrs->heads[i+1]);
	  char *colon, *ocolon;

	  NS_ASSERTION(head, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
	  if (!head) continue;

	  /* Quick hack to skip over BSD Mailbox delimiter. */
	  if (i == 0 && head[0] == 'F' && !strncmp(head, "From ", 5))
		continue;

	  /* Find the colon. */
	  for (colon = head; colon < end; colon++)
		if (*colon == ':') break;

	  if (colon >= end) continue;

	  /* Back up over whitespace before the colon. */
	  ocolon = colon;
	  for (; colon > head && nsCRT::IsAsciiSpace(colon[-1]); colon--)
		;

	  /* If the strings aren't the same length, it doesn't match. */
	  if (name_length != colon - head )
		continue;

	  /* If the strings differ, it doesn't match. */
	  if (nsCRT::strncasecmp(header_name, head, name_length))
		continue;

	  /* Otherwise, we've got a match. */
	  {
		char *contents = ocolon + 1;
		char *s;

		/* Skip over whitespace after colon. */
		while (contents <= end && nsCRT::IsAsciiSpace(*contents))
		  contents++;

		/* If we're supposed to strip at the frist token, pull `end' back to
		   the first whitespace or ';' after the first token.
		 */
		if (strip_p)
		  {
			for (s = contents;
				 s <= end && *s != ';' && *s != ',' && !nsCRT::IsAsciiSpace(*s);
				 s++)
			  ;
			end = s;
		  }

		/* Now allocate some storage.
		   If `result' already has a value, enlarge it.
		   Otherwise, just allocate a block.
		   `s' gets set to the place where the new data goes.
		 */
		if (!result)
		  {
			result = (char *) PR_MALLOC(end - contents + 1);
			if (!result)
			  return 0;
			s = result;
		  }
		else
		  {
			PRInt32 L = strlen(result);
			s = (char *) PR_Realloc(result, (L + (end - contents + 10)));
			if (!s)
			  {
				PR_Free(result);
				return 0;
			  }
			result = s;
			s = result + L;

			/* Since we are tacking more data onto the end of the header
			   field, we must make it be a well-formed continuation line,
                          by separating the old and new data with CR-LF-TAB.
			 */
			*s++ = ',';				/* #### only do this for addr headers? */
			*s++ = MSG_LINEBREAK[0];
# if (MSG_LINEBREAK_LEN == 2)
			*s++ = MSG_LINEBREAK[1];
# endif
			*s++ = '\t';
		  }

		/* Take off trailing whitespace... */
		while (end > contents && nsCRT::IsAsciiSpace(end[-1]))
		  end--;

		if (end > contents)
		  {
		    /* Now copy the header's contents in...
		     */
		    memcpy(s, contents, end - contents);
		    s[end - contents] = 0;
		  }
		else
		  {
		    s[0] = 0;
		  }

		/* If we only wanted the first occurence of this header, we're done. */
		if (!all_p) break;
	  }
	}

  if (result && !*result)  /* empty string */
	{
	  PR_Free(result);
	  return 0;
	}

  return result;
}

char *
MimeHeaders_get_parameter (const char *header_value, const char *parm_name, 
                           char **charset, char **language)
{
  if (!header_value || !parm_name || !*header_value || !*parm_name)
    return nsnull;

  nsresult rv;
  nsCOMPtr <nsIMIMEHeaderParam> mimehdrpar = 
    do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return nsnull;

  nsXPIDLCString result;
  rv = mimehdrpar->GetParameterInternal(header_value, parm_name, charset, 
                                        language, getter_Copies(result));
  return NS_SUCCEEDED(rv) ? PL_strdup(result.get()) : nsnull; 
}

#define MimeHeaders_write(OPT,DATA,LENGTH) \
  	MimeOptions_write((OPT), (DATA), (LENGTH), PR_TRUE);


#define MimeHeaders_grow_obuffer(hdrs, desired_size) \
  ((((long) (desired_size)) >= ((long) (hdrs)->obuffer_size)) ? \
   mime_GrowBuffer ((desired_size), sizeof(char), 255, \
				   &(hdrs)->obuffer, &(hdrs)->obuffer_size) \
   : 0)

int
MimeHeaders_write_all_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt, PRBool attachment)
{
  int status = 0;
  int i;
  PRBool wrote_any_p = PR_FALSE;

  NS_ASSERTION(hdrs, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!hdrs) 
    return -1;

  /* One shouldn't be trying to read headers when one hasn't finished
     parsing them yet... but this can happen if the message ended
     prematurely, and has no body at all (as opposed to a null body,
     which is more normal.)   So, if we try to read from the headers,
     let's assume that the headers are now finished.  If they aren't
     in fact finished, then a later attempt to write to them will assert.
   */
  if (!hdrs->done_p)
  {
    hdrs->done_p = PR_TRUE;
    status = MimeHeaders_build_heads_list(hdrs);
    if (status < 0) return 0;
  }

  char *charset = nsnull;
  if (opt->format_out == nsMimeOutput::nsMimeMessageSaveAs)
  {
    if (opt->override_charset)
      charset = PL_strdup(opt->default_charset);
    else
    {
      char *contentType = MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
      if (contentType)
        charset = MimeHeaders_get_parameter(contentType, HEADER_PARM_CHARSET, nsnull, nsnull);
      PR_FREEIF(contentType);
    }
  }

  for (i = 0; i < hdrs->heads_size; i++)
  {
    char *head = hdrs->heads[i];
    char *end = (i == hdrs->heads_size-1
                      ? hdrs->all_headers + hdrs->all_headers_fp
                      : hdrs->heads[i+1]);
    char *colon, *ocolon;
    char *contents = end;
    
    /* Hack for BSD Mailbox delimiter. */
    if (i == 0 && head[0] == 'F' && !strncmp(head, "From ", 5))
    {
      /* For now, we don't really want this header to be output so
         we are going to just continue */
      continue;
      /* colon = head + 4; contents = colon + 1; */
    }
    else
    {
      /* Find the colon. */
      for (colon = head; colon < end && *colon != ':'; colon++)
        ;
        
        /* Back up over whitespace before the colon. */
        ocolon = colon;
        for (; colon > head && nsCRT::IsAsciiSpace(colon[-1]); colon--)
          ;
        
        contents = ocolon + 1;
    }
    
    /* Skip over whitespace after colon. */
    while (contents < end && nsCRT::IsAsciiSpace(*contents))
      contents++;
    
    /* Take off trailing whitespace... */
    while (end > contents && nsCRT::IsAsciiSpace(end[-1]))
      end--;
    
    nsCAutoString name(Substring(head, colon));
    nsCAutoString hdr_value;

    if ( (end - contents) > 0 )
    {
      hdr_value = Substring(contents, end);
    }
    
    MimeHeaders_convert_header_value(opt, hdr_value);
    // if we're saving as html, we need to convert headers from utf8 to message charset, if any
    if (opt->format_out == nsMimeOutput::nsMimeMessageSaveAs && charset)
    {
      nsCAutoString convertedStr;
      if (NS_SUCCEEDED(ConvertFromUnicode(charset, NS_ConvertUTF8toUTF16(hdr_value),
                       convertedStr)))
      {
        hdr_value = convertedStr;
      }
    }

    if (attachment)
      status = mimeEmitterAddAttachmentField(opt, name.get(), hdr_value.get());
    else
      status = mimeEmitterAddHeaderField(opt, name.get(), hdr_value.get());
    
    if (status < 0) return status;
    if (!wrote_any_p) 
      wrote_any_p = (status > 0);
  }
  mimeEmitterAddAllHeaders(opt, hdrs->all_headers, hdrs->all_headers_fp);
  PR_FREEIF(charset);

  return 1;
}

#ifdef MOZ_SECURITY
HG99401
#endif /* MOZ_SECURITY */

/* Strip CR+LF runs within (original).
   Since the string at (original) can only shrink,
   this conversion is done in place. (original)
   is returned. */
extern "C" char *
MIME_StripContinuations(char *original)
{
	char *p1, *p2;

	/* If we were given a null string, return it as is */
	if (!original) return NULL;

	/* Start source and dest pointers at the beginning */
	p1 = p2 = original;

	while(*p2)
	{
		/* p2 runs ahead at (CR and/or LF) */
		if ((p2[0] == nsCRT::CR) || (p2[0] == nsCRT::LF))
		{
            p2++;
		} else {
            *p1++ = *p2++;
        }
	}
	*p1 = '\0';

	return original;
}

extern PRInt16 INTL_DefaultMailToWinCharSetID(PRInt16 csid);

/* Given text purporting to be a qtext header value, strip backslashes that
	may be escaping other chars in the string. */
char *
mime_decode_filename(char *name, const char *charset,
                     MimeDisplayOptions *opt)
{
  nsresult rv;
  nsCOMPtr <nsIMIMEHeaderParam> mimehdrpar = 
    do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return nsnull;
  nsCAutoString result;
  rv = mimehdrpar->DecodeParameter(nsDependentCString(name), charset,
                                   opt->default_charset,
                                   opt->override_charset, result);
  return NS_SUCCEEDED(rv) ? PL_strdup(result.get()) : nsnull; 
}

/* Pull the name out of some header or another.  Order is:
   Content-Disposition: XXX; filename=NAME (RFC 1521/1806)
   Content-Type: XXX; name=NAME (RFC 1341)
   Content-Name: NAME (no RFC, but seen to occur)
   X-Sun-Data-Name: NAME (no RFC, but used by MailTool)
 */
char *
MimeHeaders_get_name(MimeHeaders *hdrs, MimeDisplayOptions *opt)
{
  char *s = 0, *name = 0, *cvt = 0;
  char *charset = nsnull; // for RFC2231 support

  s = MimeHeaders_get(hdrs, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);
  if (s)
  {
    name = MimeHeaders_get_parameter(s, HEADER_PARM_FILENAME, &charset, NULL);
    PR_Free(s);
  }

  if (! name)
  {
    s = MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
    if (s)
    {
      nsMemory::Free(charset);

      name = MimeHeaders_get_parameter(s, HEADER_PARM_NAME, &charset, NULL);
      PR_Free(s);
    }
  }

  if (! name)
    name = MimeHeaders_get (hdrs, HEADER_CONTENT_NAME, PR_FALSE, PR_FALSE);
  
  if (! name)
    name = MimeHeaders_get (hdrs, HEADER_X_SUN_DATA_NAME, PR_FALSE, PR_FALSE);

  if (name)
  {
    /* First remove continuation delimiters (CR+LF+space), then
       remove escape ('\\') characters, then attempt to decode
       mime-2 encoded-words. The latter two are done in 
       mime_decode_filename. 
    */
    MIME_StripContinuations(name);

    /* Argh. What we should do if we want to be robust is to decode qtext
       in all appropriate headers. Unfortunately, that would be too scary
       at this juncture. So just decode qtext/mime2 here. */
    cvt = mime_decode_filename(name, charset, opt);

    nsMemory::Free(charset);

    if (cvt && cvt != name)
    {
      PR_Free(name);
      name = cvt;
    }
  }

  return name;
}

#ifdef XP_UNIX
/* This piece of junk is so that I can use BBDB with Mozilla.
   = Put bbdb-srv.perl on your path.
   = Put bbdb-srv.el on your lisp path.
   = Make sure gnudoit (comes with xemacs) is on your path.
   = Put (gnuserv-start) in ~/.emacs
   = setenv NS_MSG_DISPLAY_HOOK bbdb-srv.perl
 */
void
MimeHeaders_do_unix_display_hook_hack(MimeHeaders *hdrs)
{
  static char *cmd = 0;
  if (!cmd)
  {
  /* The first time we're invoked, look up the command in the
    environment.  Use "" as the `no command' tag. */
    cmd = getenv("NS_MSG_DISPLAY_HOOK");
    if (!cmd)
      cmd = "";
    else
      cmd = nsCRT::strdup(cmd);
  }
  
  /* Invoke "cmd" at the end of a pipe, and give it the headers on stdin.
	 The command is expected to be safe from hostile input!!
  */
  if (cmd && *cmd)
  {
    FILE *fp = popen(cmd, "w");
    if (fp)
    {
      fwrite(hdrs->all_headers, 1, hdrs->all_headers_fp, fp);
      pclose(fp);
    }
  }
}
#endif /* XP_UNIX */

static void
MimeHeaders_compact (MimeHeaders *hdrs)
{
  NS_ASSERTION(hdrs, "1.22 <rhp@netscape.com> 22 Aug 1999 08:48");
  if (!hdrs) return;

  PR_FREEIF(hdrs->obuffer);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

  /* These really shouldn't have gotten out of whack again. */
  NS_ASSERTION(hdrs->all_headers_fp <= hdrs->all_headers_size &&
            hdrs->all_headers_fp + 100 > hdrs->all_headers_size, "1.22 <rhp@netscape.com> 22 Aug 1999 08:48");
}

/* Writes the headers as text/plain.
   This writes out a blank line after the headers, unless
   dont_write_content_type is true, in which case the header-block
   is not closed off, and none of the Content- headers are written.
 */
int
MimeHeaders_write_raw_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt,
							   PRBool dont_write_content_type)
{
  int status;

  if (hdrs && !hdrs->done_p)
	{
	  hdrs->done_p = PR_TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  if (!dont_write_content_type)
	{
	  char nl[] = MSG_LINEBREAK;
	  if (hdrs)
		{
		  status = MimeHeaders_write(opt, hdrs->all_headers,
									 hdrs->all_headers_fp);
		  if (status < 0) return status;
		}
	  status = MimeHeaders_write(opt, nl, strlen(nl));
	  if (status < 0) return status;
	}
  else if (hdrs)
	{
	  PRInt32 i;
	  for (i = 0; i < hdrs->heads_size; i++)
		{
		  char *head = hdrs->heads[i];
		  char *end = (i == hdrs->heads_size-1
					   ? hdrs->all_headers + hdrs->all_headers_fp
					   : hdrs->heads[i+1]);

		  NS_ASSERTION(head, "1.22 <rhp@netscape.com> 22 Aug 1999 08:48");
		  if (!head) continue;

		  /* Don't write out any Content- header. */
		  if (!nsCRT::strncasecmp(head, "Content-", 8))
			continue;

		  /* Write out this (possibly multi-line) header. */
		  status = MimeHeaders_write(opt, head, end - head);
		  if (status < 0) return status;
		}
	}

  if (hdrs)
  	MimeHeaders_compact (hdrs);

  return 0;
}

// XXX Fix this XXX //
char *
MimeHeaders_open_crypto_stamp(void)
{
  return nsnull;
}

char *
MimeHeaders_finish_open_crypto_stamp(void)
{
  return nsnull;
}

char *
MimeHeaders_close_crypto_stamp(void)
{
  return nsnull;
}

char *
MimeHeaders_make_crypto_stamp(PRBool encrypted_p,
                              PRBool signed_p,
                              PRBool good_p,
                              PRBool unverified_p,
                              PRBool close_parent_stamp_p,
                              const char *stamp_url)
{
  return nsnull;
}
