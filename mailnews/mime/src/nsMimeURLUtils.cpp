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
#ifndef XP_MAC
#include "nsTextFragment.h"
#endif
#include "msgCore.h"
#include "mimebuf.h"
#include "nsMimeURLUtils.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/* this function will be used by the factory to generate an RFC-822 Parser....*/
nsresult NS_NewMimeURLUtils(nsIMimeURLUtils ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeURLUtils* utils = new nsMimeURLUtils();
		if (utils)
			return utils->QueryInterface(nsIMimeURLUtils::GetIID(), (void **)aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsMimeURLUtils)
NS_IMPL_RELEASE(nsMimeURLUtils)
NS_IMPL_QUERY_INTERFACE(nsMimeURLUtils, nsIMimeURLUtils::GetIID()); /* we need to pass in the interface ID of this interface */

nsMimeURLUtils::nsMimeURLUtils()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMimeURLUtils::~nsMimeURLUtils()
{
}

char *
FindAmbitiousMailToTag(const char *line, PRInt32 line_size)
{
  char  *atLoc;
  char  *workLine;

  // Sanity...
  if ( (!line) || (!*line) )
    return NULL;

  // Should I bother at all...
  if ( !(atLoc = PL_strnchr(line, '@', line_size)) )
    return NULL;

  // create a working copy...
  workLine = PL_strndup(line, line_size);
  if (!workLine)
    return NULL;

  char *ptr = PL_strchr(workLine, '@');
  if (!ptr)
    return NULL;

  *(ptr+1) = '\0';
  --ptr;
  while (ptr >= workLine)
  {
    if (IS_SPACE(*ptr) ||
				  *ptr == '<' || *ptr == '>' ||
          *ptr == '`' || *ptr == ')' ||
          *ptr == '\'' || *ptr == '"' ||
          *ptr == ']' || *ptr == '}'
          )
          break;
    --ptr;
  }

  ++ptr;

  if (*ptr == '@') 
  {
    PR_FREEIF(workLine);
    return NULL;
  }

  PRInt32           retType;
  nsMimeURLUtils    util;

  util.URLType(ptr, &retType);
  if (retType != 0)
  {
    PR_FREEIF(workLine);
    return NULL;
  }

  char *retVal = PL_strdup(ptr);
  PR_FREEIF(workLine);
  return retVal;
}

nsresult
AmbitiousURLType(const char *URL, PRInt32  *retType, const char *newURLTag)
{
  *retType = 0;

  if (!PL_strncasecmp(URL, newURLTag, PL_strlen(newURLTag))) 
  {
    *retType = MAILTO_TYPE_URL;
  }

  return NS_OK;
}

/* from libnet/mkutils.c */
nsresult 
nsMimeURLUtils::URLType(const char *URL, PRInt32  *retType)
{
  if (!URL || (URL && *URL == '\0'))
    return(NS_ERROR_NULL_POINTER);
  
  switch(*URL) {
  case 'a':
  case 'A':
    if(!PL_strncasecmp(URL,"about:security", 14))
    {
		    *retType = SECURITY_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"about:",6))
    {
		    *retType = ABOUT_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'f':
  case 'F':
    if ( (!PL_strncasecmp(URL,"ftp:",4)) ||
         (!PL_strncasecmp(URL,"ftp.",4)) )
    {
		    *retType = FTP_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"file:",5))
    {
		    *retType = FILE_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'g':
  case 'G':
    if(!PL_strncasecmp(URL,"gopher:",7)) 
    {
      *retType = GOPHER_TYPE_URL;
      return NS_OK;
    }
    break;
  case 'h':
  case 'H':
    if(!PL_strncasecmp(URL,"http:",5))
    {
		    *retType = HTTP_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"https:",6))
    {
		    *retType = SECURE_HTTP_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'i':
  case 'I':
    if(!PL_strncasecmp(URL,"internal-gopher-",16))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-news-",14))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-edit-",14))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-attachment-",20))
    {
      *retType = INTERNAL_IMAGE_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-dialog-handler",23))
    {
      *retType = HTML_DIALOG_HANDLER_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-panel-handler",22))
    {
      *retType = HTML_PANEL_HANDLER_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"internal-security-",18))
    {
      *retType = INTERNAL_SECLIB_TYPE_URL;
      return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"IMAP:",5))
    {
    	*retType = IMAP_TYPE_URL;
      return NS_OK;
    }
    break;
  case 'j':
  case 'J':
    if(!PL_strncasecmp(URL, "javascript:",11))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'l':
  case 'L':
    if(!PL_strncasecmp(URL, "livescript:",11))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'm':
  case 'M':
    if(!PL_strncasecmp(URL,"mailto:",7)) 
    {
		    *retType = MAILTO_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"mailbox:",8))
    {
		    *retType = MAILBOX_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL, "mocha:",6))
    {
		    *retType = MOCHA_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'n':
  case 'N':
    if(!PL_strncasecmp(URL,"news:",5))
    {
		    *retType = NEWS_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'p':
  case 'P':
    if(!PL_strncasecmp(URL,"pop3:",5))
    {
		    *retType = POP3_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'r':
  case 'R':
    if(!PL_strncasecmp(URL,"rlogin:",7))
    {
        *retType = RLOGIN_TYPE_URL;
        return NS_OK;
    }
    break;
  case 's':
  case 'S':
    if(!PL_strncasecmp(URL,"snews:",6))
    {
		    *retType = NEWS_TYPE_URL;
        return NS_OK;
    }
  case 't':
  case 'T':
    if(!PL_strncasecmp(URL,"telnet:",7))
    {
		    *retType = TELNET_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"tn3270:",7))
    {
		    *retType = TN3270_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'v':
  case 'V':
    if(!PL_strncasecmp(URL, VIEW_SOURCE_URL_PREFIX, 
      sizeof(VIEW_SOURCE_URL_PREFIX)-1))
    {
		    *retType = VIEW_SOURCE_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'w':
  case 'W':
    if(!PL_strncasecmp(URL,"wais:",5))
    {
		    *retType = WAIS_TYPE_URL;
        return NS_OK;
    }
    else if(!PL_strncasecmp(URL,"www.",4))
    {
		    *retType = HTTP_TYPE_URL;
        return NS_OK;
    }
    break;
  case 'u':
  case 'U':
    if(!PL_strncasecmp(URL,"URN:",4))
    {
		    *retType = URN_TYPE_URL;
        return NS_OK;
    }
    break;
    
    }
    
    /* no type match :( */
    *retType = 0;
    return NS_OK;
}

PRBool
ItMatches(const char *line, PRInt32 lineLen, const char *rep)
{
  if ( (!rep) || (!*rep) || (!line) || (!*line) )
    return PR_FALSE;

  PRInt32 compLen = PL_strlen(rep);

  if (lineLen < compLen)
    return PR_FALSE;

  if (!PL_strncasecmp(line, rep, compLen))
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
GlyphHit(const char *line, PRInt32 line_size, char **outputHTML, PRInt32 *glyphTextLen)
{

  if ( ItMatches(line, line_size, ":-)") || ItMatches(line, line_size, ":)") ) 
  {
    *outputHTML = PL_strdup("<img SRC=\"chrome://messenger/skin/smile.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }  
  else if ( ItMatches(line, line_size, ":-(") || ItMatches(line, line_size, ":(") ) 
  {
    *outputHTML = PL_strdup("<img SRC=\"chrome://messenger/skin/frown.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatches(line, line_size, ";-)"))
  {
    *outputHTML = PL_strdup("<img SRC=\"chrome://messenger/skin/wink.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }
  else if (ItMatches(line, line_size, ";-P"))
  {
    *outputHTML = PL_strdup("<img SRC=\"chrome://messenger/skin/sick.gif\" height=17 width=17 align=ABSCENTER>");
    if (!(*outputHTML))
      return PR_FALSE;
    *glyphTextLen = 3;
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
IsThisAnAmbitiousLinkType(char *link, char *mailToTag, char **linkPrefix)
{
  if (!PL_strncasecmp(link, "www.", 4))
  {
    *linkPrefix = PL_strdup("http://");
    return PR_TRUE;
  }  
  else if (!PL_strncasecmp(link, "ftp.", 4))
  {
    *linkPrefix = PL_strdup("ftp://");
    return PR_TRUE;
  }
  else if (mailToTag && *mailToTag)
  {
    if (!PL_strncasecmp(link, mailToTag, PL_strlen(mailToTag)))
    {
      *linkPrefix = PL_strdup("mailto:");
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsMimeURLUtils::ScanForURLs(const char *input, PRInt32 input_size,
                            char *output, int output_size, PRBool urls_only)
{
  int col = 0;
  const char *cp;
  const char *end = input + input_size;
  char *output_ptr = output;
  char *end_of_buffer = output + output_size - 40; /* add safty zone :( */
  PRBool line_is_citation = PR_FALSE;
  const char *cite_open1, *cite_close1;
  const char *cite_open2, *cite_close2;
  const char* color = NULL;

  char *mailToTag = NULL;

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

  PRBool      do_glyph_substitution = PR_TRUE;
  nsresult    rv;
  
  // See if we should get cute with text to glyph replacement
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs)
    prefs->GetBoolPref("mail.do_glyph_substitution", &do_glyph_substitution);

  mailToTag = FindAmbitiousMailToTag(input, (PRInt32)input_size);

  /* Normal lines are scanned for buried references to URL's
     Unfortunately, it may screw up once in a while (nobody's perfect)
   */
  for(cp = input; cp < end && output_ptr < end_of_buffer; cp++)
	{
	  /* if URLType returns true then it is most likely a URL
		 But only match protocol names if at the very beginning of
		 the string, or if the preceeding character was not alphanumeric;
		 this lets us match inside "---HTTP://XXX" but not inside of
		 things like "NotHTTP://xxx"
	   */
	  int     type = 0;
    PRBool  ambitiousHit;
    char    *glyphHTML;
    PRInt32 glyphTextLen;

    if ((do_glyph_substitution) && GlyphHit(cp, (PRInt32)end - (PRInt32)cp, &glyphHTML, &glyphTextLen))
    {
      PRInt32   size_available = output_size - (output_ptr-output);
      PR_snprintf(output_ptr, size_available, glyphHTML);
      output_ptr += PL_strlen(glyphHTML);
      cp += glyphTextLen-1;
      PR_FREEIF(glyphHTML);
      continue;
    }

    ambitiousHit = PR_FALSE;
    if (mailToTag)
      AmbitiousURLType(cp, &type, mailToTag);
    if (!type)
      URLType(cp, &type);
    else
      ambitiousHit = PR_TRUE;

	  if(!IS_SPACE(*cp) &&
		 (cp == input || (!IS_ALPHA(cp[-1]) && !IS_DIGIT(cp[-1]))) &&
		 (type) != 0)
		{
		  const char *cp2;
		  for(cp2=cp; cp2 < end; cp2++)
			{
			  /* These characters always mark the end of the URL. */
			  if (IS_SPACE(*cp2) ||
				  *cp2 == '<' || *cp2 == '>' ||
				  *cp2 == '`' || *cp2 == ')' ||
				  *cp2 == '\'' || *cp2 == '"' ||
				  /**cp2 == ']' || */ *cp2 == '}'
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
      PRBool invalidHit = PR_FALSE;

      if ( (cp2 > cp && cp2[-1] == ':') ||
			      !PL_strncmp(cp, "internal-", 9) )
        invalidHit = PR_TRUE;

      if (!invalidHit)
      {
        if ((ambitiousHit) && ((cp2-cp) < (PRInt32)PL_strlen(mailToTag)))
          invalidHit = PR_TRUE;
        if ( (!ambitiousHit) && (cp2-cp < 7) )
          invalidHit = PR_TRUE;
      }

      if (invalidHit)
			{
        nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr += (cp2-cp);
			  *output_ptr = 0;
			}
		  else
			{
			  char      *quoted_url;
        PRBool    rawLink = PR_FALSE;
			  PRInt32   size_left = output_size - (output_ptr-output);
        char      *linkPrefix = NULL;

			  if(cp2-cp > size_left)
				return NS_ERROR_OUT_OF_MEMORY;

			  nsCRT::memcpy(output_ptr, cp, cp2-cp);
			  output_ptr[cp2-cp] = 0;

        rawLink = IsThisAnAmbitiousLinkType(output_ptr, mailToTag, &linkPrefix);
			  quoted_url = nsEscapeHTML(output_ptr);
			  if (!quoted_url) return NS_ERROR_OUT_OF_MEMORY;

        if (rawLink)
			    PR_snprintf(output_ptr, size_left,
						    "<A HREF=\"%s%s\">%s</A>",
                linkPrefix,
						    quoted_url,
						    quoted_url);
        else
			    PR_snprintf(output_ptr, size_left,
						    "<A HREF=\"%s\">%s</A>",
						    quoted_url,
						    quoted_url);

			  output_ptr += PL_strlen(output_ptr);
			  PR_Free(quoted_url);
        PR_FREEIF(linkPrefix);
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

      PR_FREEIF(mailToTag);
      mailToTag = FindAmbitiousMailToTag(cp, (PRInt32)end - (PRInt32)cp);
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

  PR_FREEIF(mailToTag);
  return NS_OK;
}

/* modifies a url of the form   /foo/../foo1  ->  /foo1
 *
 * it only operates on "file" "ftp" and "http" url's all others are returned
 * unmodified
 *
 * returns the modified passed in URL string
 */
nsresult
nsMimeURLUtils::ReduceURL (char *url, char **retURL)
{
  PRInt32     url_type;

  URLType(url, &url_type);
  char * fwd_ptr;
  char * url_ptr;
  char * path_ptr;
  
  if(!url)
  {
    *retURL = url;
    return NS_OK;
  }
  
  if(url_type == HTTP_TYPE_URL || url_type == FILE_TYPE_URL ||
	   url_type == FTP_TYPE_URL ||
     url_type == SECURE_HTTP_TYPE_URL)
  {
    
		/* find the path so we only change that and not the host
    */
    path_ptr = PL_strchr(url, '/');
    
    if(!path_ptr)
    {
      *retURL = url;
      return NS_OK;
    }
    
    if(*(path_ptr+1) == '/')
		    path_ptr = PL_strchr(path_ptr+2, '/');
    
    if(!path_ptr)
    {
      *retURL = url;
      return NS_OK;
    }
    
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
  
  *retURL = url;
  return NS_OK;
}

/* 
 * Makes a relative URL into an absolute one.  
 *
 * If an absolute url is passed in it will be copied and returned.
 *
 * Always returns a malloc'd string or NULL on out of memory error
 */
nsresult
nsMimeURLUtils::MakeAbsoluteURL(char * absolute_url, const char * relative_url, char **retURL)
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
    *retURL = ret_url;
    return NS_OK;
  }
  
  /* use the URLType function to figure
	 * out if it's a recognized URL method
	 */
  URLType(relative_url, &url_type);
  
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
      *retURL = PL_strdup("");
      return NS_OK;
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
      *retURL = ret_url;
      return NS_OK;
		  }
    else
		  {
      /* it's a screwed up relative url of the form http:[relative url]
      * remove the stuff before and at the colon and treat it as a normal
      * relative url
      */
      char * colon2 = PL_strchr(relative_url, ':');
      
      relative_url = colon2 + 1;
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
  URLType(absolute_url, &base_type);
  if ((base_type == MAILBOX_TYPE_URL || base_type == IMAP_TYPE_URL) &&
    *relative_url != '#' &&
    *relative_url != '?')
  {
    *retURL = PL_strdup("");
    return NS_OK;
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
        
        return(ReduceURL(ret_url, retURL));
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
    {
      *retURL = NULL;
      return NS_ERROR_OUT_OF_MEMORY;   /* out of memory */
    }
    
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
  
  return ReduceURL(ret_url, retURL);
}

static void
Append(char** output, PRInt32* output_max, char** curoutput, const char* buf, PRInt32 length)
{
  if (length + (*curoutput) - (*output) >= *output_max) {
    int offset = (*curoutput) - (*output);
    do {
      (*output_max) += 1024;
    } while (length + (*curoutput) - (*output) >= *output_max);
    *output = (char *)PR_Realloc(*output, *output_max);
    if (!*output) return;
    *curoutput = *output + offset;
  }
  nsCRT::memcpy(*curoutput, buf, length);
  *curoutput += length;
}

nsresult
nsMimeURLUtils::ScanHTMLForURLs(const char* input, char **retBuf)
{
    char* output = NULL;
    char* curoutput;
    PRInt32 output_max;
    char* tmpbuf = NULL;
    PRInt32 tmpbuf_max;
    PRInt32 inputlength;
    const char* inputend;
    const char* linestart;
    const char* lineend;

    PR_ASSERT(input);
    if (!input) 
    {
      *retBuf = NULL;
      return NS_OK;
    }
    inputlength = PL_strlen(input);

    output_max = inputlength + 1024; /* 1024 bytes ought to be enough to quote
                                        several URLs, which ought to be as many
                                        as most docs use. */
    output = (char *)PR_Malloc(output_max);
    if (!output) goto FAIL;

    tmpbuf_max = 1024;
    tmpbuf = (char *)PR_Malloc(tmpbuf_max);
    if (!tmpbuf) goto FAIL;

    inputend = input + inputlength;

    linestart = input;
    curoutput = output;


    /* Here's the strategy.  We find a chunk of plainish looking text -- no
       embedded CR or LF, no "<" or "&".  We feed that off to ScanForURLs,
       and append the result.  Then we skip to the next bit of plain text.  If
       we stopped at an "&", go to the terminating ";".  If we stopped at a
       "<", well, if it was a "<A>" tag, then skip to the closing "</A>".
       Otherwise, skip to the end of the tag.
       */


    lineend = linestart;
    while (linestart < inputend && lineend <= inputend) {
        switch (*lineend) {
        case '<':
        case '>':
        case '&':
        case CR:
        case LF:
        case '\0':
            if (lineend > linestart) {
                int length = lineend - linestart;
                if (length * 3 > tmpbuf_max) {
                    tmpbuf_max = length * 3 + 512;
                    PR_Free(tmpbuf);
                    tmpbuf = (char *)PR_Malloc(tmpbuf_max);
                    if (!tmpbuf) goto FAIL;
                }
                if (ScanForURLs(linestart, length,
                                tmpbuf, tmpbuf_max, TRUE) != NS_OK) {
                    goto FAIL;
                }
                length = PL_strlen(tmpbuf);
                Append(&output, &output_max, &curoutput, tmpbuf, length);
                if (!output) goto FAIL;

            }
            linestart = lineend;
            lineend = NULL;
            if (inputend - linestart < 5) {
                /* Too little to worry about; shove the rest out. */
                lineend = inputend;
            } else {
                switch (*linestart) {
                case '<':
                    if ((linestart[1] == 'a' || linestart[1] == 'A') &&
                        linestart[2] == ' ') {
                        lineend = PL_strcasestr(linestart, "</a");
                        if (lineend) {
                            lineend = PL_strchr(lineend, '>');
                            if (lineend) lineend++;
                        }
                    } else {
                        lineend = PL_strchr(linestart, '>');
                        if (lineend) lineend++;
                    }
                    break;
                case '&':
                    lineend = PL_strchr(linestart, ';');
                    if (lineend) lineend++;
                    break;
                default:
                    lineend = linestart + 1;
                    break;
                }
            }
            if (!lineend) lineend = inputend;
            Append(&output, &output_max, &curoutput, linestart,
                   lineend - linestart);
            if (!output) goto FAIL;
            linestart = lineend;
            break;
        default:
            lineend++;
        }
    }
    PR_Free(tmpbuf);
    tmpbuf = NULL;
    *curoutput = '\0';
    *retBuf = output;

FAIL:
    if (tmpbuf) PR_Free(tmpbuf);
    if (output) PR_Free(output);
    *retBuf = NULL;
    return NS_OK;
}

//
// Also skip '>' as part of host name 
//
nsresult
nsMimeURLUtils::ParseURL(const char *url, int parts_requested, char **returnVal)
{
  char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
  char *atSign, *host, *passwordColon, *gtThan;
  
  assert(url); 
  if(!url)
  {
    *returnVal = mime_SACat(&rv, "");
    return NS_ERROR_FAILURE;
  }
  colon = PL_strchr(url, ':'); /* returns a const char */
  
  /* Get the protocol part, not including anything beyond the colon */
  if (parts_requested & GET_PROTOCOL_PART) {
    if(colon) {
      char val = *(colon+1);
      *(colon+1) = '\0';
      mime_SACopy(&rv, url);
      *(colon+1) = val;
      
      /* If the user wants more url info, tack on extra slashes. */
      if( (parts_requested & GET_HOST_PART)
        || (parts_requested & GET_USERNAME_PART)
        || (parts_requested & GET_PASSWORD_PART)
        )
      {
        if( *(colon+1) == '/' && *(colon+2) == '/')
          mime_SACat(&rv, "//");
        /* If there's a third slash consider it file:/// and tack on the last slash. */
        if( *(colon+3) == '/' )
          mime_SACat(&rv, "/");
      }
		  }
  }
  
  
  /* Get the username if one exists */
  if (parts_requested & GET_USERNAME_PART) {
    if (colon 
      && (*(colon+1) == '/')
      && (*(colon+2) == '/')
      && (*(colon+3) != '\0')) {
      
      if ( (slash = PL_strchr(colon+3, '/')) != NULL)
        *slash = '\0';
      if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
        *atSign = '\0';
        if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL)
          *passwordColon = '\0';
        mime_SACat(&rv, colon+3);
        
        /* Get the password if one exists */
        if (parts_requested & GET_PASSWORD_PART) {
          if (passwordColon) {
            mime_SACat(&rv, ":");
            mime_SACat(&rv, passwordColon+1);
          }
        }
        if (parts_requested & GET_HOST_PART)
          mime_SACat(&rv, "@");
        if (passwordColon)
          *passwordColon = ':';
        *atSign = '@';
      }
      if (slash)
        *slash = '/';
    }
  }
  
  /* Get the host part */
  if (parts_requested & GET_HOST_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/')
      {
        slash = PL_strchr(colon+3, '/');
        
        if(slash)
          *slash = '\0';
        
        if( (atSign = PL_strchr(colon+3, '@')) != NULL)
          host = atSign+1;
        else
          host = colon+3;
        
        ques_mark = PL_strchr(host, '?');
        
        if(ques_mark)
          *ques_mark = '\0';
        
        gtThan = PL_strchr(host, '>');
        
        if (gtThan)
          *gtThan = '\0';
        
          /*
          * Protect systems whose header files forgot to let the client know when
          * gethostbyname would trash their stack.
        */
#ifndef MAXHOSTNAMELEN
#ifdef XP_OS2
#error Managed to get into NET_ParseURL without defining MAXHOSTNAMELEN !!!
#endif
#define MAXHOSTNAMELEN  256
#endif
        /* limit hostnames to within MAXHOSTNAMELEN characters to keep
        * from crashing
        */
        if(PL_strlen(host) > MAXHOSTNAMELEN)
        {
          char * cp;
          char old_char;
          
          cp = host+MAXHOSTNAMELEN;
          old_char = *cp;
          
          *cp = '\0';
          
          mime_SACat(&rv, host);
          
          *cp = old_char;
        }
        else
        {
          mime_SACat(&rv, host);
        }
        
        if(slash)
          *slash = '/';
        
        if(ques_mark)
          *ques_mark = '?';
        
        if (gtThan)
          *gtThan = '>';
      }
    }
  }
  
  /* Get the path part */
  if (parts_requested & GET_PATH_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/')
      {
        /* skip host part */
        slash = PL_strchr(colon+3, '/');
      }
      else 
      {
      /* path is right after the colon
        */
        slash = colon+1;
      }
      
      if(slash)
      {
        ques_mark = PL_strchr(slash, '?');
        hash_mark = PL_strchr(slash, '#');
        
        if(ques_mark)
          *ques_mark = '\0';
        
        if(hash_mark)
          *hash_mark = '\0';
        
        mime_SACat(&rv, slash);
        
        if(ques_mark)
          *ques_mark = '?';
        
        if(hash_mark)
          *hash_mark = '#';
      }
		  }
  }
		
  if(parts_requested & GET_HASH_PART) {
    hash_mark = PL_strchr(url, '#'); /* returns a const char * */
    
    if(hash_mark)
		  {
      ques_mark = PL_strchr(hash_mark, '?');
      
      if(ques_mark)
        *ques_mark = '\0';
      
      mime_SACat(&rv, hash_mark);
      
      if(ques_mark)
        *ques_mark = '?';
		  }
  }
  
  if(parts_requested & GET_SEARCH_PART) {
    ques_mark = PL_strchr(url, '?'); /* returns a const char * */
    
    if(ques_mark)
    {
      hash_mark = PL_strchr(ques_mark, '#');
      
      if(hash_mark)
        *hash_mark = '\0';
      
      mime_SACat(&rv, ques_mark);
      
      if(hash_mark)
        *hash_mark = '#';
    }
  }
  
  /* copy in a null string if nothing was copied in
	 */
  if(!rv)
	   mime_SACopy(&rv, "");

  *returnVal = rv;

  return NS_OK;
}
