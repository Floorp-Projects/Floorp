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

/* 
 * netlib.c - external dependencies on the old netlib
 */

#include "xp.h"
#include "rosetta.h"

#include "prmem.h"
#include "plstr.h"

#define CONST const

extern int MK_OUT_OF_MEMORY;

/* from libnet/mkutils.c */
PUBLIC char *
NET_EscapeHTML(const char * string)
{
    char *rv = (char *) PR_MALLOC(PL_strlen(string)*4 + 1); /* The +1 is for
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

# define MSG_Prefs void
# define MSG_GetCitationStyle(w,x,y,z) do{}while(0)
# define MSG_GetPrefs(x) 0

/* from libnet/mkutils.c */
PUBLIC int
NET_ScanForURLs(
				MSG_Pane* pane,
				const char *input, int32 input_size,
				char *output, int output_size, XP_Bool urls_only)
{
  int col = 0;
  const char *cp;
  const char *end = input + input_size;
  char *output_ptr = output;
  char *end_of_buffer = output + output_size - 40; /* add safty zone :( */
  Bool line_is_citation = PR_FALSE;
  const char *cite_open1, *cite_close1;
  const char *cite_open2, *cite_close2;
  const char* color = NULL;

  if (urls_only)
	{
	  cite_open1 = cite_close1 = "";
	  cite_open2 = cite_close2 = "";
	}
  else
	{
#ifdef MOZILLA_CLIENT
	  MSG_Prefs* prefs;
	  MSG_FONT font = MSG_ItalicFont;
	  MSG_CITATION_SIZE size = MSG_NormalSize;

	  if (pane) {
		prefs = MSG_GetPrefs(pane);
		MSG_GetCitationStyle(prefs, &font, &size, &color);
	  }
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
		  PR_ASSERT(0);
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
		  PR_ASSERT(0);
		  cite_open2 = cite_close2 = "";
		  break;
		}
#else  /* !MOZILLA_CLIENT */
		PR_ASSERT(0);
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
			   !PL_strncmp (input, ">From ", 6))	/* #$%^ing sendmail... */
		;
	  else if (*s == '>' || *s == ']')
		{
		  line_is_citation = PR_TRUE;
		  PL_strcpy(output_ptr, cite_open1);
		  output_ptr += PL_strlen(cite_open1);
		  PL_strcpy(output_ptr, cite_open2);
		  output_ptr += PL_strlen(cite_open2);
		  if (color &&
			  output_ptr + PL_strlen(color) + 20 < end_of_buffer) {
			PL_strcpy(output_ptr, "<FONT COLOR=");
			output_ptr += PL_strlen(output_ptr);
			PL_strcpy(output_ptr, color);
			output_ptr += PL_strlen(output_ptr);
			PL_strcpy(output_ptr, ">");
			output_ptr += PL_strlen(output_ptr);
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
				commas_ok = PR_TRUE;
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
			  !PL_strncmp(cp, "internal-", 9))
			{
			  XP_MEMCPY(output_ptr, cp, cp2-cp);
			  output_ptr += (cp2-cp);
			  *output_ptr = 0;
			}
		  else
			{
			  char *quoted_url;
			  PRInt32 size_left = output_size - (output_ptr-output);

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
			  output_ptr += PL_strlen(output_ptr);
			  PR_Free(quoted_url);
			  output_ptr += PL_strlen(output_ptr);
			}

		  cp = cp2-1;  /* go to next word */
		}
	  else
		{
		  /* Make sure that special symbols don't screw up the HTML parser
		   */
		  if(*cp == '<')
			{
			  PL_strcpy(output_ptr, "&lt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '>')
			{
			  PL_strcpy(output_ptr, "&gt;");
			  output_ptr += 4;
			  col++;
			}
		  else if(*cp == '&')
			{
			  PL_strcpy(output_ptr, "&amp;");
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
		PL_strcpy(output_ptr, "</FONT>");
		output_ptr += PL_strlen(output_ptr);
	  }

	  PL_strcpy(output_ptr, cite_close2);
	  output_ptr += PL_strlen (cite_close2);
	  PL_strcpy(output_ptr, cite_close1);
	  output_ptr += PL_strlen (cite_close1);
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
		if(!PL_strncasecmp(URL,"about:security", 14))
		    return(SECURITY_TYPE_URL);
		else if(!PL_strncasecmp(URL,"about:",6))
		    return(ABOUT_TYPE_URL);
		break;
	case 'f':
	case 'F':
		if(!PL_strncasecmp(URL,"ftp:",4))
		    return(FTP_TYPE_URL);
		else if(!PL_strncasecmp(URL,"file:",5))
		    return(FILE_TYPE_URL);
		break;
	case 'g':
	case 'G':
    	if(!PL_strncasecmp(URL,"gopher:",7)) 
	        return(GOPHER_TYPE_URL);
		break;
	case 'h':
	case 'H':
		if(!PL_strncasecmp(URL,"http:",5))
		    return(HTTP_TYPE_URL);
		else if(!PL_strncasecmp(URL,"https:",6))
		    return(SECURE_HTTP_TYPE_URL);
		break;
	case 'i':
	case 'I':
		if(!PL_strncasecmp(URL,"internal-gopher-",16))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-news-",14))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-edit-",14))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-attachment-",20))
			return(INTERNAL_IMAGE_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-dialog-handler",23))
			return(HTML_DIALOG_HANDLER_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-panel-handler",22))
			return(HTML_PANEL_HANDLER_TYPE_URL);
		else if(!PL_strncasecmp(URL,"internal-security-",18))
			return(INTERNAL_SECLIB_TYPE_URL);
    	else if(!PL_strncasecmp(URL,"IMAP:",5))
    		return(IMAP_TYPE_URL);
		break;
	case 'j':
	case 'J':
		if(!PL_strncasecmp(URL, "javascript:",11))
		    return(MOCHA_TYPE_URL);
		break;
	case 'l':
	case 'L':
		if(!PL_strncasecmp(URL, "livescript:",11))
		    return(MOCHA_TYPE_URL);
		break;
	case 'm':
	case 'M':
    	if(!PL_strncasecmp(URL,"mailto:",7)) 
		    return(MAILTO_TYPE_URL);
		else if(!PL_strncasecmp(URL,"mailbox:",8))
		    return(MAILBOX_TYPE_URL);
		else if(!PL_strncasecmp(URL, "mocha:",6))
		    return(MOCHA_TYPE_URL);
		break;
	case 'n':
	case 'N':
		if(!PL_strncasecmp(URL,"news:",5))
		    return(NEWS_TYPE_URL);
#ifdef NFS_TYPE_URL
		else if(!PL_strncasecmp(URL,"nfs:",4))
		    return(NFS_TYPE_URL);
#endif /* NFS_TYPE_URL */
		break;
	case 'p':
	case 'P':
		if(!PL_strncasecmp(URL,"pop3:",5))
		    return(POP3_TYPE_URL);
		break;
	case 'r':
	case 'R':
		if(!PL_strncasecmp(URL,"rlogin:",7))
		    return(RLOGIN_TYPE_URL);
		break;
	case 's':
	case 'S':
		if(!PL_strncasecmp(URL,"snews:",6))
		    return(NEWS_TYPE_URL);
	case 't':
	case 'T':
		if(!PL_strncasecmp(URL,"telnet:",7))
		    return(TELNET_TYPE_URL);
		else if(!PL_strncasecmp(URL,"tn3270:",7))
		    return(TN3270_TYPE_URL);
		break;
	case 'v':
	case 'V':
		if(!PL_strncasecmp(URL, VIEW_SOURCE_URL_PREFIX, 
							  sizeof(VIEW_SOURCE_URL_PREFIX)-1))
		    return(VIEW_SOURCE_TYPE_URL);
		break;
	case 'w':
	case 'W':
		if(!PL_strncasecmp(URL,"wais:",5))
		    return(WAIS_TYPE_URL);
		break;
	case 'u':
	case 'U':
		if(!PL_strncasecmp(URL,"URN:",4))
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
    PRInt32         extra = 0;
    char        *hexChars = "0123456789ABCDEF";

	if(!str)
		return(0);

    for(src=((unsigned char *)str); *src; src++)
	  {
        if (!IS_OK(*src))
            extra+=2; /* the escape, plus an extra byte for each nibble */
	  }

    if(!(result = (char *) PR_MALLOC((PRInt32) (src - ((unsigned char *)str)) + extra + 1)))
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
		len = PL_strlen(reg_type);
        if(!PL_strncasecmp(reg_type, url, len) && url[len] == ':')
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
		path_ptr = PL_strchr(url, '/');

		if(!path_ptr)
		    return(url);

		if(*(path_ptr+1) == '/')
		    path_ptr = PL_strchr(path_ptr+2, '/');
			
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
			if(!PL_strncmp(relative_url,"internal-icon-", 14)
		   		|| !PL_strncmp(relative_url,"internal-external-reconnect:", 28)
			 		|| !PL_strcmp(relative_url,"internal-external-plugin"))
            url_type = INTERNAL_IMAGE_TYPE_URL;
            break;
    	  case '/':
        	if(!PL_strncasecmp(relative_url, "/mc-icons/", 10) ||
               !PL_strncasecmp(relative_url, "/ns-icons/", 10))
          	  {
            	if(!PL_strcmp(relative_url+10, "menu.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "unknown.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "text.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "image.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "sound.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "binary.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
            	else if(!PL_strcmp(relative_url+10, "movie.gif"))
                	url_type = INTERNAL_IMAGE_TYPE_URL;
          	  }
        	break;
	    }
	  }
	else if(url_type == ABOUT_TYPE_URL)
	  {
		/* don't allow about:cache in a document */
		if(!PL_strncasecmp(relative_url, "about:cache", 11)
		   || !PL_strncasecmp(relative_url, "about:global", 12)
		   || !PL_strncasecmp(relative_url, "about:image-cache", 17)
		   || !PL_strncasecmp(relative_url, "about:memory-cache", 18))
		  {
			return PL_strdup("");
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
		char * colon = PL_strchr(relative_url, ':'); /* must be there */

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
			char * colon = PL_strchr(relative_url, ':');

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
	if ((base_type == MAILBOX_TYPE_URL || base_type == IMAP_TYPE_URL) &&
		*relative_url != '#' &&
		*relative_url != '?')
	  {
		return PL_strdup("");
	  }

	if(relative_url[0] == '/' && relative_url[1] == '/')
	  {
		/* a host absolute URL
		 */

		/* find the colon after the protocol
		 */
		cat_point = PL_strchr(absolute_url, ':');
		if (cat_point && base_type == WYSIWYG_TYPE_URL)
			cat_point = PL_strchr(cat_point + 1, ':');

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
        char *colon = PL_strchr(absolute_url, ':');
		if (colon && base_type == WYSIWYG_TYPE_URL)
			colon = PL_strchr(colon + 1, ':');

		if(colon)
		  {
			if(colon[1] == '/' && colon[2] == '/')
			  {
				/* find the next slash 
				 */
				cat_point = PL_strchr(colon+3, '/');

				if(!cat_point)
				  {
				    /* if there isn't another slash then the cat point is the very end
				     */
					cat_point = &absolute_url[PL_strlen(absolute_url)];
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
		char * hash = PL_strchr(absolute_url, '#');
	
		if(hash)
		  {
			char * ques_mark = PL_strchr(absolute_url, '?');
   
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
			cat_point = &absolute_url[PL_strlen(absolute_url)]; /* the end of the URL */
		  }
	  }
	else
	  {
		/* a completely relative URL
		 *
		 * append after the last slash
		 */
		char * ques = PL_strchr(absolute_url, '?');
		char * hash = PL_strchr(absolute_url, '#');

		if(ques)
			*ques = '\0';

		if(hash)
			*hash = '\0';

		cat_point = PL_strrchr(absolute_url, '/');

		/* if there are no slashes then append right after the colon after the protocol
		 */
		if(!cat_point)
		    cat_point = PL_strchr(absolute_url, ':');

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
        new_length = PL_strlen(absolute_url) + PL_strlen(relative_url) + 1;
        ret_url = (char *) PR_MALLOC(new_length);
		if(!ret_url)
			return(NULL);  /* out of memory */

        PL_strcpy(ret_url, absolute_url);
        PL_strcat(ret_url, relative_url);
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

/*	Very similar to strdup except it free's too
 */
PUBLIC char * 
NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    PR_Free(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
PUBLIC char *
NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

PUBLIC int
NET_UnEscapeCnt (char * str)
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

} /* NET_UnEscapeCnt */

PUBLIC char *
NET_UnEscape(char * str)
{
	(void)NET_UnEscapeCnt(str);

	return str;
}

#define ISHEX(c) ( ((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F') )
#define NONHEX(c) (!ISHEX(c))

PUBLIC char * 
escape_unescaped_percents(const char *incomingURL)
{
	const char *inC;
	char *outC;
	char *result = (char *) PR_Malloc(PL_strlen(incomingURL)*3+1);

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

#define INITIAL_MAX_ALL_HEADERS 10

/* malloc, init, and set the URL structure used
 * for the network library
 */
PUBLIC URL_Struct * 
NET_CreateURLStruct (CONST char *url, NET_ReloadMethod force_reload)
{
    PRUint32 all_headers_size;
    URL_Struct * URL_s  = PR_NEW(URL_Struct);

    if(!URL_s)
	  {
		return NULL;
	  }

    /* zap the whole structure */
    memset (URL_s, 0, sizeof (URL_Struct));

    URL_s->SARCache = NULL;

    /* we haven't modified the address at all (since we are just creating it)*/
    URL_s->address_modified=NO;

	URL_s->method = URL_GET_METHOD;

    URL_s->force_reload = force_reload;

	URL_s->load_background = PR_FALSE;

    URL_s->ref_count = 1;

    /* Allocate space for pointers to hold message (http, news etc) headers */
    all_headers_size = 
	INITIAL_MAX_ALL_HEADERS * sizeof (URL_s->all_headers.key[0]);
    URL_s->all_headers.key = (char **) PR_Malloc(all_headers_size);
    if(!URL_s->all_headers.key)
      {
	NET_FreeURLStruct(URL_s);
	return NULL;
      }
    memset (URL_s->all_headers.key, 0, all_headers_size);

    all_headers_size = 
	INITIAL_MAX_ALL_HEADERS * sizeof (URL_s->all_headers.value[0]);
    URL_s->all_headers.value = (char **) PR_Malloc(all_headers_size);
    if(!URL_s->all_headers.value)
      {
	NET_FreeURLStruct(URL_s);
	return NULL;
      }
    memset (URL_s->all_headers.value, 0, all_headers_size);

    URL_s->all_headers.max_index = INITIAL_MAX_ALL_HEADERS;

	URL_s->owner_data = NULL;
	URL_s->owner_id   = 0;

    /* set the passed in value */
    StrAllocCopy(URL_s->address, url);
	if (!URL_s->address) {
		/* out of memory, clean up previously allocated objects */
		NET_FreeURLStruct(URL_s);
		URL_s = NULL;
	}

#ifdef TRUST_LABELS
	/* add the newly minted URL_s struct to the list of the same so
	 * that when cookie to trust label matching occurs I can find the
	 * associated trust list */
	if ( URL_s && net_URL_sList ) {
		LIBNET_LOCK();
		XP_ListAddObjectToEnd( net_URL_sList, URL_s );
		LIBNET_UNLOCK();
	}
#endif

    return(URL_s);
}

/* free the contents of AllHeader structure that is part of URL_Struct
 */
PRIVATE void
net_FreeURLAllHeaders (URL_Struct * URL_s)
{
    PRUint32 i=0;

    /* Free all memory associated with header information */
    for (i = 0; i < URL_s->all_headers.empty_index; i++) {
	if (URL_s->all_headers.key) {
	    PR_FREEIF(URL_s->all_headers.key[i]);
	}
	if (URL_s->all_headers.value) {
	    PR_FREEIF(URL_s->all_headers.value[i]);
	}
    }
    URL_s->all_headers.empty_index = URL_s->all_headers.max_index = 0;

    PR_FREEIF(URL_s->all_headers.key);
    PR_FREEIF(URL_s->all_headers.value);
    URL_s->all_headers.key = URL_s->all_headers.value = NULL;
}


/* free the contents and the URL structure when finished
 */
PUBLIC void
NET_FreeURLStruct (URL_Struct * URL_s)
{
    if(!URL_s)
	return;
    
    /* if someone is holding onto a pointer to us don't die */
    PR_ASSERT(URL_s->ref_count > 0);
    URL_s->ref_count--;
    if(URL_s->ref_count > 0)
	return;

#ifdef TRUST_LABELS
	/* remove URL_s struct from the list of the same */
	if ( URL_s && net_URL_sList ) {
		LIBNET_LOCK();
	   	XP_ListRemoveObject( net_URL_sList, URL_s );
		LIBNET_UNLOCK();
	}
#endif

    PR_FREEIF(URL_s->address);
    PR_FREEIF(URL_s->username);
    PR_FREEIF(URL_s->password);
    PR_FREEIF(URL_s->IPAddressString);
    PR_FREEIF(URL_s->referer);
    PR_FREEIF(URL_s->post_data);
    PR_FREEIF(URL_s->post_headers);
    PR_FREEIF(URL_s->content_type);
    PR_FREEIF(URL_s->content_encoding);
    PR_FREEIF(URL_s->x_mac_type);
    PR_FREEIF(URL_s->x_mac_creator);
	PR_FREEIF(URL_s->charset);
	PR_FREEIF(URL_s->boundary);
    PR_FREEIF(URL_s->redirecting_url);
	PR_FREEIF(URL_s->authenticate);
	PR_FREEIF(URL_s->protection_template);
    PR_FREEIF(URL_s->http_headers);
	PR_FREEIF(URL_s->cache_file);
    PR_FREEIF(URL_s->window_target);
	PR_FREEIF(URL_s->window_chrome);
	PR_FREEIF(URL_s->refresh_url);
	PR_FREEIF(URL_s->wysiwyg_url);
	PR_FREEIF(URL_s->etag);
	PR_FREEIF(URL_s->origin_url);
	PR_FREEIF(URL_s->error_msg);

    HG87376

    /* Free all memory associated with header information */
    net_FreeURLAllHeaders(URL_s);

	if(URL_s->files_to_post)
	  {
		/* free a null terminated array of filenames
		 */
		int i=0;
		for(i=0; URL_s->files_to_post[i] != NULL; i++)
		  {
			PR_FREEIF(URL_s->files_to_post[i]); /* free the filenames */
		  }
		PR_FREEIF(URL_s->files_to_post); /* free the array */
	  }

    /* Like files_to_post. */
	if(URL_s->post_to)
	  {
		/* free a null terminated array of filenames
		 */
		int i=0;
		for(i=0; URL_s->post_to[i] != NULL; i++)
		  {
			PR_FREEIF(URL_s->post_to[i]); /* free the URLs */
		  }
		PR_FREEIF(URL_s->post_to); /* free the array */
	  }


  /* free the array */
  PR_FREEIF(URL_s->add_crlf);

	 PR_FREEIF(URL_s->page_services_url);

	if (URL_s->privacy_policy_url != (char*)(-1)) {
		PR_FREEIF(URL_s->privacy_policy_url);
	} else {
		URL_s->privacy_policy_url = NULL;
	}

#ifdef TRUST_LABELS
	/* delete the entries and the trust list, if the list was created then there
	 * are entries on it.  */
	if ( URL_s->TrustList != NULL ) {
		/* delete each trust list entry then delete the list */
		TrustLabel *ALabel;
		XP_List *tempList = URL_s->TrustList;
		while ( (ALabel = XP_ListRemoveEndObject( tempList )) != NULL ) {
			TL_Destruct( ALabel );
		}
		XP_ListDestroy( URL_s->TrustList );
		URL_s->TrustList = NULL;
    }
#endif

    PR_Free(URL_s);
}


