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

/* mimestub.c --- junk to let libmime.a be tested standalone.
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

/* 
   mimestub.c --- junk to let libmime.a be tested standalone.

   The junk in this file is various bits and pieces that have been
   cut-and-pasted from other places in Mozilla, mainly netlib.  We just
   copied these functions to avoid pulling in the spaghetti that is netlib,
   and libxp, and libmsg, and and and...

   In addition to this file, we link directly against lib/xp/xp_file.o,
   lib/xp/xp_str.o, and lib/libmsg/addr.o, because those files *actually
   stand on their own*!

   Life kinda s#$%s, but oh well.
 */

#include "xp.h"


#ifndef XP_UNIX
ERROR!	this is a unix-only file for the "mimefilt" standalone program.
		This does not go into libmime.a.
#endif


#include <netdb.h>

#define CONST const

extern int MK_MSG_MIME_MAC_FILE;
extern int MK_MSG_NO_HEADERS;
extern int MK_OUT_OF_MEMORY;
extern int MK_UNABLE_TO_OPEN_TMP_FILE;

/* recommended by comment in lib/xp/xp_intl.c... */
char *
XP_GetString(int16 i)
{
	extern char * XP_GetBuiltinString(int16 i);

	return XP_GetBuiltinString(i);
}

int16 INTL_CharSetNameToID(char *charset) { return 0; }
char *XP_GetStringForHTML (int i, int16 wincsid, char* english)
{
  return english; 
}


/* from nspr somewhere...
 */
struct hostent *
PR_gethostbyname(const char *name)
{
  return gethostbyname(name);
}



/* from libnet/mkutils.c */
PUBLIC char *
NET_EscapeHTML(const char * string)
{
    char *rv = (char *) XP_ALLOC(XP_STRLEN(string)*4 + 1); /* The +1 is for
															  the trailing
															  null! */
	char *ptr = rv;

	if(rv)
	  {
		for(; *string != '\0'; string++)
		  {
			if(*string == '<')
			  {
				*ptr++ = '&';
				*ptr++ = 'l';
				*ptr++ = 't';
				*ptr++ = ';';
			  }
			else if(*string == '>')
			  {
				*ptr++ = '&';
				*ptr++ = 'g';
				*ptr++ = 't';
				*ptr++ = ';';
			  }
			else if(*string == '&')
			  {
				*ptr++ = '&';
				*ptr++ = 'a';
				*ptr++ = 'm';
				*ptr++ = 'p';
				*ptr++ = ';';
			  }
			else
			  {
				*ptr++ = *string;
			  }
		  }
		*ptr = '\0';
	  }

	return(rv);
}


/* from libnet/mkutils.c */
# define MSG_FONT           int
# define MSG_PlainFont      0
# define MSG_BoldFont       1
# define MSG_ItalicFont     2
# define MSG_BoldItalicFont 3
# define MSG_CITATION_SIZE  int
# define MSG_NormalSize     4
# define MSG_Bigger         5
# define MSG_Smaller        6
static int MSG_CitationFont = MSG_ItalicFont;
static int MSG_CitationSize = MSG_NormalSize;
static const char *MSG_CitationColor = 0;

#ifndef MOZILLA_30
# define MSG_Prefs void
# define MSG_GetCitationStyle(w,x,y,z) do{}while(0)
# define MSG_GetPrefs(x) 0
#endif

/* from libnet/mkutils.c */
PUBLIC int
NET_ScanForURLs(
#ifndef MOZILLA_30
				MSG_Pane* pane,
#endif /* !MOZILLA_30 */
				const char *input, int32 input_size,
				char *output, int output_size, XP_Bool urls_only)
{
  int col = 0;
  const char *cp;
  const char *end = input + input_size;
  char *output_ptr = output;
  char *end_of_buffer = output + output_size - 40; /* add safty zone :( */
  Bool line_is_citation = FALSE;
  const char *cite_open1, *cite_close1;
  const char *cite_open2, *cite_close2;
#ifndef MOZILLA_30
  const char* color = NULL;
#else  /* MOZILLA_30 */
  const char* color = MSG_CitationColor;
#endif /* MOZILLA_30 */

  if (urls_only)
	{
	  cite_open1 = cite_close1 = "";
	  cite_open2 = cite_close2 = "";
	}
  else
	{
#ifdef MOZILLA_CLIENT
# ifdef MOZILLA_30
	  MSG_FONT font = MSG_CitationFont;
	  MSG_CITATION_SIZE size = MSG_CitationSize;
# else  /* !MOZILLA_30 */
	  MSG_Prefs* prefs;
	  MSG_FONT font = MSG_ItalicFont;
	  MSG_CITATION_SIZE size = MSG_NormalSize;

	  if (pane) {
		prefs = MSG_GetPrefs(pane);
		MSG_GetCitationStyle(prefs, &font, &size, &color);
	  }
#endif /* !MOZILLA_30 */
	  switch (font)
		{
		case MSG_PlainFont:
		  cite_open1 = "", cite_close1 = "";
		  break;
		case MSG_BoldFont:
		  cite_open1 = "<B>", cite_close1 = "</B>";
		  break;
		case MSG_ItalicFont:
		  cite_open1 = "<I>", cite_close1 = "</I>";
		  break;
		case MSG_BoldItalicFont:
		  cite_open1 = "<B><I>", cite_close1 = "</I></B>";
		  break;
		default:
		  XP_ASSERT(0);
		  cite_open1 = cite_close1 = "";
		  break;
		}
	  switch (size)
		{
		case MSG_NormalSize:
		  cite_open2 = "", cite_close2 = "";
		  break;
		case MSG_Bigger:
		  cite_open2 = "<FONT SIZE=\"+1\">", cite_close2 = "</FONT>";
		  break;
		case MSG_Smaller:
		  cite_open2 = "<FONT SIZE=\"-1\">", cite_close2 = "</FONT>";
		  break;
		default:
		  XP_ASSERT(0);
		  cite_open2 = cite_close2 = "";
		  break;
		}
#else  /* !MOZILLA_CLIENT */
		XP_ASSERT(0);
#endif /* !MOZILLA_CLIENT */
	}

  if (!urls_only)
	{
	  /* Decide whether this line is a quotation, and should be italicized.
		 This implements the following case-sensitive regular expression:

			^[ \t]*[A-Z]*[]>]

	     Which matches these lines:

		    > blah blah blah
		         > blah blah blah
		    LOSER> blah blah blah
		    LOSER] blah blah blah
	   */
	  const char *s = input;
	  while (s < end && XP_IS_SPACE (*s)) s++;
	  while (s < end && *s >= 'A' && *s <= 'Z') s++;

	  if (s >= end)
		;
	  else if (input_size >= 6 && *s == '>' &&
			   !XP_STRNCMP (input, ">From ", 6))	/* #$%^ing sendmail... */
		;
	  else if (*s == '>' || *s == ']')
		{
		  line_is_citation = TRUE;
		  XP_STRCPY(output_ptr, cite_open1);
		  output_ptr += XP_STRLEN(cite_open1);
		  XP_STRCPY(output_ptr, cite_open2);
		  output_ptr += XP_STRLEN(cite_open2);
		  if (color &&
			  output_ptr + XP_STRLEN(color) + 20 < end_of_buffer) {
			XP_STRCPY(output_ptr, "<FONT COLOR=");
			output_ptr += XP_STRLEN(output_ptr);
			XP_STRCPY(output_ptr, color);
			output_ptr += XP_STRLEN(output_ptr);
			XP_STRCPY(output_ptr, ">");
			output_ptr += XP_STRLEN(output_ptr);
		  }
		}
	}

  /* Normal lines are scanned for buried references to URL's
     Unfortunately, it may screw up once in a while (nobody's perfect)
   */
  for(cp = input; cp < end && output_ptr < end_of_buffer; cp++)
	{
	  /* if NET_URL_Type returns true then it is most likely a URL
		 But only match protocol names if at the very beginning of
		 the string, or if the preceeding character was not alphanumeric;
		 this lets us match inside "---HTTP://XXX" but not inside of
		 things like "NotHTTP://xxx"
	   */
	  int type = 0;
	  if(!XP_IS_SPACE(*cp) &&
		 (cp == input || (!XP_IS_ALPHA(cp[-1]) && !XP_IS_DIGIT(cp[-1]))) &&
		 (type = NET_URL_Type(cp)) != 0)
		{
		  const char *cp2;
#if 0
		  Bool commas_ok = (type == MAILTO_TYPE_URL);
#endif

		  for(cp2=cp; cp2 < end; cp2++)
			{
			  /* These characters always mark the end of the URL. */
			  if (XP_IS_SPACE(*cp2) ||
				  *cp2 == '<' || *cp2 == '>' ||
				  *cp2 == '`' || *cp2 == ')' ||
				  *cp2 == '\'' || *cp2 == '"' ||
				  *cp2 == ']' || *cp2 == '}'
#if 0
				  || *cp2 == '!'
#endif
				  )
				break;

#if 0
			  /* But a "," marks the end of the URL only if there was no "?"
				 earlier in the URL (this is so we can do imagemaps, like
				 "foo.html?300,400".)
			   */
			  else if (*cp2 == '?')
				commas_ok = TRUE;
			  else if (*cp2 == ',' && !commas_ok)
				break;
#endif
			}

		  /* Check for certain punctuation characters on the end, and strip
			 them off. */
		  while (cp2 > cp && 
				 (cp2[-1] == '.' || cp2[-1] == ',' || cp2[-1] == '!' ||
				  cp2[-1] == ';' || cp2[-1] == '-' || cp2[-1] == '?' ||
				  cp2[-1] == '#'))
			cp2--;

		  col += (cp2 - cp);

		  /* if the url is less than 7 characters then we screwed up
		   * and got a "news:" url or something which is worthless
		   * to us.  Exclude the A tag in this case.
		   *
		   * Also exclude any URL that ends in a colon; those tend
		   * to be internal and magic and uninteresting.
		   *
		   * And also exclude the builtin icons, whose URLs look
		   * like "internal-gopher-binary".
		   */
		  if (cp2-cp < 7 ||
			  (cp2 > cp && cp2[-1] == ':') ||
			  !XP_STRNCMP(cp, "internal-", 9))
			{
			  XP_MEMCPY(output_ptr, cp, cp2-cp);
			  output_ptr += (cp2-cp);
			  *output_ptr = 0;
			}
		  else
			{
			  char *quoted_url;
			  int32 size_left = output_size - (output_ptr-output);

			  if(cp2-cp > size_left)
				return MK_OUT_OF_MEMORY;

			  XP_MEMCPY(output_ptr, cp, cp2-cp);
			  output_ptr[cp2-cp] = 0;
			  quoted_url = NET_EscapeHTML(output_ptr);
			  if (!quoted_url) return MK_OUT_OF_MEMORY;
			  PR_snprintf(output_ptr, size_left,
						  "<A HREF=\"%s\">%s</A>",
						  quoted_url,
						  quoted_url);
			  output_ptr += XP_STRLEN(output_ptr);
			  XP_FREE(quoted_url);
			  output_ptr += XP_STRLEN(output_ptr);
			}

		  cp = cp2-1;  /* go to next word */
		}
	  else
		{
		  /* Make sure that special symbols don't screw up the HTML parser
		   */
		  if(*cp == '<')
			{
			  XP_STRCPY(output_ptr, "&lt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '>')
			{
			  XP_STRCPY(output_ptr, "&gt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '&')
			{
			  XP_STRCPY(output_ptr, "&amp;");
			  output_ptr += 5;
			  col++;
			}
		  else
			{
			  *output_ptr++ = *cp;
			  col++;
			}
		}
	}

  *output_ptr = 0;

  if (line_is_citation)	/* Close off the highlighting */
	{
	  if (color) {
		XP_STRCPY(output_ptr, "</FONT>");
		output_ptr += XP_STRLEN(output_ptr);
	  }

	  XP_STRCPY(output_ptr, cite_close2);
	  output_ptr += XP_STRLEN (cite_close2);
	  XP_STRCPY(output_ptr, cite_close1);
	  output_ptr += XP_STRLEN (cite_close1);
	}

  return 0;
}


/* from libnet/mkutils.c */
PUBLIC int 
NET_URL_Type (CONST char *URL)
{
    /* Protect from SEGV */
    if (!URL || (URL && *URL == '\0'))
        return(0);

    switch(*URL) {
    case 'a':
    case 'A':
		if(!strncasecomp(URL,"about:security", 14))
		    return(SECURITY_TYPE_URL);
		else if(!strncasecomp(URL,"about:",6))
		    return(ABOUT_TYPE_URL);
		break;
	case 'f':
	case 'F':
		if(!strncasecomp(URL,"ftp:",4))
		    return(FTP_TYPE_URL);
		else if(!strncasecomp(URL,"file:",5))
		    return(FILE_TYPE_URL);
		break;
	case 'g':
	case 'G':
    	if(!strncasecomp(URL,"gopher:",7)) 
	        return(GOPHER_TYPE_URL);
		break;
	case 'h':
	case 'H':
		if(!strncasecomp(URL,"http:",5))
		    return(HTTP_TYPE_URL);
		else if(!strncasecomp(URL,"https:",6))
		    return(SECURE_HTTP_TYPE_URL);
		break;
	case 'i':
	case 'I':
		if(!strncasecomp(URL,"internal-gopher-",16))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!strncasecomp(URL,"internal-news-",14))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!strncasecomp(URL,"internal-edit-",14))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!strncasecomp(URL,"internal-attachment-",20))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!strncasecomp(URL,"internal-dialog-handler",23))
			return(HTML_DIALOG_HANDLER_TYPE_URL);
		else if(!strncasecomp(URL,"internal-panel-handler",22))
			return(HTML_PANEL_HANDLER_TYPE_URL);
		else if(!strncasecomp(URL,"internal-security-",18))
			return(INTERNAL_SECLIB_TYPE_URL);
		break;
	case 'j':
	case 'J':
		if(!strncasecomp(URL, "javascript:",11))
		    return(MOCHA_TYPE_URL);
		break;
	case 'l':
	case 'L':
		if(!strncasecomp(URL, "livescript:",11))
		    return(MOCHA_TYPE_URL);
		break;
	case 'm':
	case 'M':
    	if(!strncasecomp(URL,"mailto:",7)) 
		    return(MAILTO_TYPE_URL);
		else if(!strncasecomp(URL,"mailbox:",8))
		    return(MAILBOX_TYPE_URL);
		else if(!strncasecomp(URL, "mocha:",6))
		    return(MOCHA_TYPE_URL);
		break;
	case 'n':
	case 'N':
		if(!strncasecomp(URL,"news:",5))
		    return(NEWS_TYPE_URL);
#ifdef NFS_TYPE_URL
		else if(!strncasecomp(URL,"nfs:",4))
		    return(NFS_TYPE_URL);
#endif /* NFS_TYPE_URL */
		break;
	case 'p':
	case 'P':
		if(!strncasecomp(URL,"pop3:",5))
		    return(POP3_TYPE_URL);
		break;
	case 'r':
	case 'R':
		if(!strncasecomp(URL,"rlogin:",7))
		    return(RLOGIN_TYPE_URL);
		break;
	case 's':
	case 'S':
		if(!strncasecomp(URL,"snews:",6))
		    return(NEWS_TYPE_URL);
	case 't':
	case 'T':
		if(!strncasecomp(URL,"telnet:",7))
		    return(TELNET_TYPE_URL);
		else if(!strncasecomp(URL,"tn3270:",7))
		    return(TN3270_TYPE_URL);
		break;
	case 'v':
	case 'V':
		if(!strncasecomp(URL, VIEW_SOURCE_URL_PREFIX, 
							  sizeof(VIEW_SOURCE_URL_PREFIX)-1))
		    return(VIEW_SOURCE_TYPE_URL);
		break;
	case 'w':
	case 'W':
		if(!strncasecomp(URL,"wais:",5))
		    return(WAIS_TYPE_URL);
		break;
	case 'u':
	case 'U':
		if(!strncasecomp(URL,"URN:",4))
		    return(URN_TYPE_URL);
		break;
	  
    }

    /* no type match :( */
    return(0);
}



/* from libnet/mkparse.c */
PRIVATE CONST 
int netCharType[256] =
/*	Bit 0		xalpha		-- the alphas
**	Bit 1		xpalpha		-- as xalpha but 
**                             converts spaces to plus and plus to %20
**	Bit 3 ...	path		-- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 1x */
		 0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?	 */
	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_	 */
	     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno	 */
	     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,	/* 7X  pqrstuvwxyz{\}~	DEL */
		 0, };

#define IS_OK(C) (netCharType[((unsigned int) (C))] & (mask))


#define HEX_ESCAPE '%'


PUBLIC char *
NET_Escape (const char * str, int mask)
{
    register CONST unsigned char *src;
    register unsigned char *dst;
    char        *result;
    int32         extra = 0;
    char        *hexChars = "0123456789ABCDEF";

	if(!str)
		return(0);

    for(src=((unsigned char *)str); *src; src++)
	  {
        if (!IS_OK(*src))
            extra+=2; /* the escape, plus an extra byte for each nibble */
	  }

    if(!(result = (char *) XP_ALLOC((int32) (src - ((unsigned char *)str)) + extra + 1)))
        return(0);

    dst = (unsigned char *) result;
    for(src=((unsigned char *)str); *src; src++)
    if (IS_OK((unsigned char) (*src)))
	  {
        *dst++ = *src;
	  }
	else if(mask == URL_XPALPHAS && *src == ' ')
	  {
        *dst++ = '+'; /* convert spaces to pluses */
	  }
    else 
	  {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexChars[(*src >> 4) & 0x0f];  /* high nibble */
        *dst++ = hexChars[*src & 0x0f];     /* low nibble */
      }

    *dst++ = '\0';     /* tack on eos */
    return result;
}


/* from libmsg/msgutils.c */
int
msg_GrowBuffer (uint32 desired_size, uint32 element_size, uint32 quantum,
				char **buffer, uint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  uint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Make sure we don't choke on WIN16 */
		  XP_ASSERT(0);
		  return MK_OUT_OF_MEMORY;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) XP_REALLOC (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) XP_ALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		return MK_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}


XP_List * ExternalURLTypeList=0;

#define EXTERNAL_TYPE_URL  999  /* a special external URL type */

/* check the passed in url to see if it is an
 * external type url
 */
PRIVATE int
net_CheckForExternalURLType(char * url)
{
    XP_List * list_ptr = ExternalURLTypeList;
    char * reg_type;
	int len;

    while((reg_type = (char *)XP_ListNextObject(list_ptr)))
      {
		len = XP_STRLEN(reg_type);
        if(!strncasecomp(reg_type, url, len) && url[len] == ':')
          { 
            /* found it */
            return EXTERNAL_TYPE_URL; /* done */ 
          }
      }

	return(0);
}

/* modifies a url of the form   /foo/../foo1  ->  /foo1
 *
 * it only operates on "file" "ftp" and "http" url's all others are returned
 * unmodified
 *
 * returns the modified passed in URL string
 */
PRIVATE char *
net_ReduceURL (char *url)
{
	int url_type = NET_URL_Type(url);
    char * fwd_ptr;
	char * url_ptr;
    char * path_ptr;

    if(!url)
        return(url);

	if(url_type == HTTP_TYPE_URL || url_type == FILE_TYPE_URL ||
	   url_type == FTP_TYPE_URL ||
	   url_type == SECURE_HTTP_TYPE_URL)
	  {

		/* find the path so we only change that and not the host
		 */
		path_ptr = XP_STRCHR(url, '/');

		if(!path_ptr)
		    return(url);

		if(*(path_ptr+1) == '/')
		    path_ptr = XP_STRCHR(path_ptr+2, '/');
			
		if(!path_ptr)
		    return(url);

		fwd_ptr = path_ptr;
		url_ptr = path_ptr;

		for(; *fwd_ptr != '\0'; fwd_ptr++)
		  {
			
			if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '/')
			  {
			    /* remove ./ 
		         */	
				fwd_ptr += 1;
			  }
			else if(*fwd_ptr == '/' && *(fwd_ptr+1) == '.' && *(fwd_ptr+2) == '.' && 
									(*(fwd_ptr+3) == '/' || *(fwd_ptr+3) == '\0'))
			  {
			    /* remove foo/.. 
		         */	
				/* reverse the url_ptr to the previous slash
				 */
				if(url_ptr != path_ptr) 
					url_ptr--; /* we must be going back at least one */
				for(;*url_ptr != '/' && url_ptr != path_ptr; url_ptr--)
					;  /* null body */
	
				/* forward the fwd_prt past the ../
				 */
				fwd_ptr += 2;
			  }
			else
			  {
				/* copy the url incrementaly 
				 */
				*url_ptr++ = *fwd_ptr;
			  }
		  }
		*url_ptr = '\0';  /* terminate the url */
	  }

	return(url);
}


/* Makes a relative URL into an absolute one.  
 *
 * If an absolute url is passed in it will be copied and returned.
 *
 * Always returns a malloc'd string or NULL on out of memory error
 */
PUBLIC char *
NET_MakeAbsoluteURL(char * absolute_url, char * relative_url)
{
	char * ret_url=0;
    int    new_length;
	char * cat_point=0;
	char   cat_point_char;
	int    url_type=0;
	int    base_type;

	/* if either is NULL
	 */
	if(!absolute_url || !relative_url)
	  {
		StrAllocCopy(ret_url, relative_url);
		return(ret_url);
	  }

	/* use the URL_Type function to figure
	 * out if it's a recognized URL method
	 */
    url_type = NET_URL_Type(relative_url);

	/* there are some extra cases we need to catch
	 */
	if(!url_type)
	  {
       switch(*relative_url) 
	    {
		  case 'i':
			if(!XP_STRNCMP(relative_url,"internal-icon-", 14)
		   		|| !XP_STRNCMP(relative_url,"internal-external-reconnect:", 28)
			 		|| !XP_STRCMP(relative_url,"internal-external-plugin"))
            url_type = INTERNAL_IMAGE_TYPE_URL;
            break;
    	  case '/':
        	if(!strncasecomp(relative_url, "/mc-icons/", 10) ||
               !strncasecomp(relative_url, "/ns-icons/", 10))
          	  {
            	if(!XP_STRCMP(relative_url+10, "menu.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "unknown.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "text.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "image.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "sound.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "binary.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!XP_STRCMP(relative_url+10, "movie.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
          	  }
        	break;
	    }
	  }
	else if(url_type == ABOUT_TYPE_URL)
	  {
		/* don't allow about:cache in a document */
		if(!strncasecomp(relative_url, "about:cache", 11)
		   || !strncasecomp(relative_url, "about:global", 12)
		   || !strncasecomp(relative_url, "about:image-cache", 17)
		   || !strncasecomp(relative_url, "about:memory-cache", 18))
		  {
			return XP_STRDUP("");
		  }
	  }

	if(!url_type)
		url_type = net_CheckForExternalURLType(relative_url);

    if(url_type)
      {
        /* it's either an absolute url
         * or a messed up one of the type proto:/path
         * but notice the missing host.
         */
		char * colon = XP_STRCHR(relative_url, ':'); /* must be there */

		if( (colon && *(colon+1) == '/' && *(colon+2) == '/') || 
			(url_type != GOPHER_TYPE_URL
			 && url_type != FTP_TYPE_URL
 			 && url_type != HTTP_TYPE_URL
			 && url_type != SECURE_HTTP_TYPE_URL
			 && url_type != RLOGIN_TYPE_URL
			 && url_type != TELNET_TYPE_URL
			 && url_type != TN3270_TYPE_URL
			 && url_type != WAIS_TYPE_URL) )
		  {
			/* it appears to have all it's parts.  Assume it's completely
			 * absolute
			 */
			StrAllocCopy(ret_url, relative_url);
			return(ret_url);
		  }
		else
		  {
			/* it's a screwed up relative url of the form http:[relative url]
			 * remove the stuff before and at the colon and treat it as a normal
			 * relative url
			 */
			char * colon = XP_STRCHR(relative_url, ':');

			relative_url = colon+1;
		  }
      }


	/* At this point, we know that `relative_url' is not absolute.
	   If the base URL, `absolute_url', is a "mailbox:" URL, then
	   we should not allow relative expansion: that is,
	   NET_MakeAbsoluteURL("mailbox:/A/B/C", "YYY") should not
	   return "mailbox:/A/B/YYY".

	   However, expansion of "#" and "?" parameters should work:
	   NET_MakeAbsoluteURL("mailbox:/A/B/C?id=XXX", "#part2")
	   should be allowed to expand to "mailbox:/A/B/C?id=XXX#part2".

	   If you allow random HREFs in attached HTML to mail messages to
	   expand to mailbox URLs, then bad things happen -- among other
	   things, an entry will be automatically created in the folders
	   list for each of these bogus non-files.

	   It's an open question as to whether relative expansion should
	   be allowed for news: and snews: URLs.

	   Reasons to allow it:

	     =  ClariNet has been using it

		 =  (Their reason:) it's the only way for an HTML news message
		    to refer to another HTML news message in a way which
			doesn't make assumptions about the name of the news host,
			and which also doesn't assume that the message is on the
			default news host.

	   Reasons to disallow it:

	     =  Consistency with "mailbox:"

		 =  If there is a news message which has type text/html, and
		    which has a relative URL like <IMG SRC="foo.gif"> in it,
			but which has no <BASE> tag, then we would expand that
			image to "news:foo.gif".  Which would fail, of course,
			but which might annoy news admins by generating bogus
			references.

	   So for now, let's allow 
	   NET_MakeAbsoluteURL("news:123@4", "456@7") => "news:456@7"; and
	   NET_MakeAbsoluteURL("news://h/123@4", "456@7") => "news://h/456@7".
	 */
	base_type = NET_URL_Type(absolute_url);
	if (base_type == MAILBOX_TYPE_URL &&
		*relative_url != '#' &&
		*relative_url != '?')
	  {
		return XP_STRDUP("");
	  }

	if(relative_url[0] == '/' && relative_url[1] == '/')
	  {
		/* a host absolute URL
		 */

		/* find the colon after the protocol
		 */
		cat_point = XP_STRCHR(absolute_url, ':');
		if (cat_point && base_type == WYSIWYG_TYPE_URL)
			cat_point = XP_STRCHR(cat_point + 1, ':');

		/* append after the colon
		 */
		if(cat_point)
		   cat_point++;

	  }
	else if(relative_url[0] == '/')
	  {
		/* a path absolute URL
         * append at the slash after the host part
		 */

		/* find the colon after the protocol 
         */
        char *colon = XP_STRCHR(absolute_url, ':');
		if (colon && base_type == WYSIWYG_TYPE_URL)
			colon = XP_STRCHR(colon + 1, ':');

		if(colon)
		  {
			if(colon[1] == '/' && colon[2] == '/')
			  {
				/* find the next slash 
				 */
				cat_point = XP_STRCHR(colon+3, '/');

				if(!cat_point)
				  {
				    /* if there isn't another slash then the cat point is the very end
				     */
					cat_point = &absolute_url[XP_STRLEN(absolute_url)];
				  }
			  }
			else
			  {
				/* no host was given so the cat_point is right after the colon
				 */
				cat_point = colon+1;
			  }

#if defined(XP_WIN) || defined(XP_OS2)
            /* this will allow drive letters to work right on windows 
             */
            if(XP_IS_ALPHA(*cat_point) && *(cat_point+1) == ':')
                cat_point += 2;
#endif /* XP_WIN */

		  }
	  }
	else if(relative_url[0] == '#')
	  {
		/* a positioning within the same document relative url
		 *
		 * add teh relative url to the full text of the absolute url minus
		 * any # punctuation the url might have
	 	 *
		 */
		char * hash = XP_STRCHR(absolute_url, '#');
	
		if(hash)
		  {
			char * ques_mark = XP_STRCHR(absolute_url, '?');
   
			if(ques_mark)
			  {
				/* this is a hack.
				 * copy things to try and make them more correct
				 */
				*hash = '\0';

				StrAllocCopy(ret_url, absolute_url);
				StrAllocCat(ret_url, relative_url);
				StrAllocCat(ret_url, ques_mark);

				*hash = '#';
				
				return(net_ReduceURL(ret_url));
			  }

			cat_point = hash;
		  }
		else
		  {
			cat_point = &absolute_url[XP_STRLEN(absolute_url)]; /* the end of the URL */
		  }
	  }
	else
	  {
		/* a completely relative URL
		 *
		 * append after the last slash
		 */
		char * ques = XP_STRCHR(absolute_url, '?');
		char * hash = XP_STRCHR(absolute_url, '#');

		if(ques)
			*ques = '\0';

		if(hash)
			*hash = '\0';

		cat_point = XP_STRRCHR(absolute_url, '/');

		/* if there are no slashes then append right after the colon after the protocol
		 */
		if(!cat_point)
		    cat_point = XP_STRCHR(absolute_url, ':');

		/* set the value back 
		 */
		if(ques)
			*ques = '?';

		if(hash)
			*hash = '#';

		if(cat_point)
			cat_point++;  /* append right after the slash or colon not on it */
	  }
	
    if(cat_point)
      {
		cat_point_char = *cat_point;  /* save the value */
        *cat_point = '\0';
        new_length = XP_STRLEN(absolute_url) + XP_STRLEN(relative_url) + 1;
        ret_url = (char *) XP_ALLOC(new_length);
		if(!ret_url)
			return(NULL);  /* out of memory */

        XP_STRCPY(ret_url, absolute_url);
        XP_STRCAT(ret_url, relative_url);
        *cat_point = cat_point_char;  /* set the absolute url back to its original state */
      } 
	else
	  {
		/* something went wrong.  just return a copy of the relative url
		 */
		StrAllocCopy(ret_url, relative_url);
	  }

	return(net_ReduceURL(ret_url));
}


/* from libmsg/msgutils.c */
static int
msg_convert_and_send_buffer(char* buf, int length, XP_Bool convert_newlines_p,
							int32 (*per_line_fn) (char *line,
												  uint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

  XP_ASSERT(buf && length > 0);
  if (!buf || length <= 0) return -1;
  newline = buf + length;
  XP_ASSERT(newline[-1] == CR || newline[-1] == LF);
  if (newline[-1] != CR && newline[-1] != LF) return -1;

  if (!convert_newlines_p)
	{
	}
#if (LINEBREAK_LEN == 1)
  else if ((newline - buf) >= 2 &&
		   newline[-2] == CR &&
		   newline[-1] == LF)
	{
	  /* CRLF -> CR or LF */
	  buf [length - 2] = LINEBREAK[0];
	  length--;
	}
  else if (newline > buf + 1 &&
		   newline[-1] != LINEBREAK[0])
	{
	  /* CR -> LF or LF -> CR */
	  buf [length - 1] = LINEBREAK[0];
	}
#else
  else if (((newline - buf) >= 2 && newline[-2] != CR) ||
		   ((newline - buf) >= 1 && newline[-1] != LF))
	{
	  /* LF -> CRLF or CR -> CRLF */
	  length++;
	  buf[length - 2] = LINEBREAK[0];
	  buf[length - 1] = LINEBREAK[1];
	}
#endif

  return (*per_line_fn)(buf, length, closure);
}


/* from libmsg/msgutils.c */
int
msg_LineBuffer (const char *net_buffer, int32 net_buffer_size,
				char **bufferP, uint32 *buffer_sizeP, uint32 *buffer_fpP,
				XP_Bool convert_newlines_p,
				int32 (*per_line_fn) (char *line, uint32 line_length,
									  void *closure),
				void *closure)
{
  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
	  net_buffer_size > 0 && net_buffer[0] != LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	XP_ASSERT(*buffer_sizeP > *buffer_fpP);
	if (*buffer_sizeP <= *buffer_fpP) return -1;
	status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										 convert_newlines_p,
										 per_line_fn, closure);
	if (status < 0) return status;
	*buffer_fpP = 0;
  }
  while (net_buffer_size > 0)
	{
	  const char *net_buffer_end = net_buffer + net_buffer_size;
	  const char *newline = 0;
	  const char *s;


	  for (s = net_buffer; s < net_buffer_end; s++)
		{
		  /* Move forward in the buffer until the first newline.
			 Stop when we see CRLF, CR, or LF, or the end of the buffer.
			 *But*, if we see a lone CR at the *very end* of the buffer,
			 treat this as if we had reached the end of the buffer without
			 seeing a line terminator.  This is to catch the case of the
			 buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
		  if (*s == CR || *s == LF)
			{
			  newline = s;
			  if (newline[0] == CR)
				{
				  if (s == net_buffer_end - 1)
					{
					  /* CR at end - wait for the next character. */
					  newline = 0;
					  break;
					}
				  else if (newline[1] == LF)
					/* CRLF seen; swallow both. */
					newline++;
				}
			  newline++;
			  break;
			}
		}

	  /* Ensure room in the net_buffer and append some or all of the current
		 chunk of data to it. */
	  {
		const char *end = (newline ? newline : net_buffer_end);
		uint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (*buffer_sizeP))
		  {
			status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		XP_MEMCPY ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
	  }

	  /* Now *bufferP contains either a complete line, or as complete
		 a line as we have read so far.

		 If we have a line, process it, and then remove it from `*bufferP'.
		 Then go around the loop again, until we drain the incoming data.
	   */
	  if (!newline)
		return 0;

	  status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										   convert_newlines_p,
										   per_line_fn, closure);
	  if (status < 0) return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}
