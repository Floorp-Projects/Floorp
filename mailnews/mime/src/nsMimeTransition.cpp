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

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsMimeTransition.h"
#include "nsTextFragment.h"
#include "msgCore.h"
#include "mimebuf.h"

extern "C" int XP_FORWARDED_MESSAGE_ATTACHMENT = 100;
extern "C" int MK_UNABLE_TO_OPEN_TMP_FILE = 101;
extern "C" int MK_OUT_OF_MEMORY = 102;
extern "C" int MK_MSG_XSENDER_INTERNAL = 103;
extern "C" int MK_MSG_USER_WROTE = 104;
extern "C" int MK_MSG_SHOW_ATTACHMENT_PANE = 105;
extern "C" int MK_MSG_NO_HEADERS = 106;
extern "C" int MK_MSG_LINK_TO_DOCUMENT = 107;
extern "C" int MK_MSG_IN_MSG_X_USER_WROTE = 108;
extern "C" int MK_MSG_DOCUMENT_INFO = 109;
extern "C" int MK_MSG_ATTACHMENT = 110;
extern "C" int MK_MSG_ADDBOOK_MOUSEOVER_TEXT = 111;
extern "C" int MK_MIMEHTML_DISP_TO = 112;
extern "C" int MK_MIMEHTML_DISP_SUBJECT = 113;
extern "C" int MK_MIMEHTML_DISP_SENDER = 114;
extern "C" int MK_MIMEHTML_DISP_RESENT_TO = 115;
extern "C" int MK_MIMEHTML_DISP_RESENT_SENDER = 116;
extern "C" int MK_MIMEHTML_DISP_RESENT_MESSAGE_ID = 117;
extern "C" int MK_MIMEHTML_DISP_RESENT_FROM = 118;
extern "C" int MK_MIMEHTML_DISP_RESENT_DATE = 119;
extern "C" int MK_MIMEHTML_DISP_RESENT_COMMENTS = 120;
extern "C" int MK_MIMEHTML_DISP_RESENT_CC = 121;
extern "C" int MK_MIMEHTML_DISP_REPLY_TO = 122;
extern "C" int MK_MIMEHTML_DISP_REFERENCES = 123;
extern "C" int MK_MIMEHTML_DISP_ORGANIZATION = 124;
extern "C" int MK_MIMEHTML_DISP_NEWSGROUPS = 125;
extern "C" int MK_MIMEHTML_DISP_MESSAGE_ID = 126;
extern "C" int MK_MIMEHTML_DISP_FROM = 127;
extern "C" int MK_MIMEHTML_DISP_FOLLOWUP_TO = 128;
extern "C" int MK_MIMEHTML_DISP_DATE = 129;
extern "C" int MK_MIMEHTML_DISP_CC = 130;
extern "C" int MK_MIMEHTML_DISP_BCC = 131;

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

extern "C" char 
*XP_GetStringForHTML (int i, PRInt16 wincsid, char* english)
{
  return english; 
}

extern "C" char 
*XP_GetString (int i)
{
  return PL_strdup("Error condition");
}


extern "C" NET_cinfo *
NET_cinfo_find_type (char *uri)
{
  return NULL;  
}

extern "C" NET_cinfo *
NET_cinfo_find_info_by_type (char *uri)
{
  return NULL;  
}

/* from libnet/mkutils.c */
PRInt32
URL_Type (const char *URL)
{
  if (!URL || (URL && *URL == '\0'))
    return(PR_FALSE);
  
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

extern "C" int
nsScanForURLs(const char *input, int32 input_size,
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
    cite_open1 = "", cite_close1 = "";
    cite_open2 = cite_close2 = "";

/********** RICHIE_CITATION
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
**********************/
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
	  while (s < end && IS_SPACE (*s)) s++;
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
	  /* if URL_Type returns true then it is most likely a URL
		 But only match protocol names if at the very beginning of
		 the string, or if the preceeding character was not alphanumeric;
		 this lets us match inside "---HTTP://XXX" but not inside of
		 things like "NotHTTP://xxx"
	   */
	  int type = 0;
	  if(!IS_SPACE(*cp) &&
		 (cp == input || (!IS_ALPHA(cp[-1]) && !IS_DIGIT(cp[-1]))) &&
		 (type = URL_Type(cp)) != 0)
		{
		  const char *cp2;
		  for(cp2=cp; cp2 < end; cp2++)
			{
			  /* These characters always mark the end of the URL. */
			  if (IS_SPACE(*cp2) ||
				  *cp2 == '<' || *cp2 == '>' ||
				  *cp2 == '`' || *cp2 == ')' ||
				  *cp2 == '\'' || *cp2 == '"' ||
				  *cp2 == ']' || *cp2 == '}'
				  )
				break;

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
        nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr += (cp2-cp);
			  *output_ptr = 0;
			}
		  else
			{
			  char *quoted_url;
			  PRInt32 size_left = output_size - (output_ptr-output);

			  if(cp2-cp > size_left)
				return MK_OUT_OF_MEMORY;

			  nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr[cp2-cp] = 0;
			  quoted_url = nsEscapeHTML(output_ptr);
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
	int url_type = URL_Type(url);
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

/* 
 * Makes a relative URL into an absolute one.  
 *
 * If an absolute url is passed in it will be copied and returned.
 *
 * Always returns a malloc'd string or NULL on out of memory error
 */
extern "C" char *
nsMakeAbsoluteURL(char * absolute_url, char * relative_url)
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
    mime_SACopy(&ret_url, relative_url);
    return(ret_url);
  }
  
  /* use the URL_Type function to figure
	 * out if it's a recognized URL method
	 */
  url_type = URL_Type(relative_url);
  
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
  
  // RICHIE - NO LONGER APPLICABLE 
  //  if(!url_type)
  // 	  url_type = net_CheckForExternalURLType(relative_url);
  
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
      mime_SACopy(&ret_url, relative_url);
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
  base_type = URL_Type(absolute_url);
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
      if(IS_ALPHA(*cat_point) && *(cat_point+1) == ':')
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
        
        mime_SACopy(&ret_url, absolute_url);
        mime_SACat(&ret_url, relative_url);
        mime_SACat(&ret_url, ques_mark);
        
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
    mime_SACopy(&ret_url, relative_url);
  }
  
  return(net_ReduceURL(ret_url));
}
