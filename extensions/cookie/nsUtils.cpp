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

#include "nsUtils.h"
#include "xp_core.h"
#include "nsXPIDLString.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsIFileSpec.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prmem.h"

static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define MAX_HOST_NAME_LEN 64
#define BUFSIZE 128
#define LOCALIZATION "chrome://communicator/locale/wallet/cookie.properties"

nsresult
ckutil_getChar(nsInputFileStream& strm, char& c) {
  static char buffer[BUFSIZE];
  static PRInt32 next = BUFSIZE, count = BUFSIZE;

  if (next == count) {
    if (BUFSIZE > count) { // never say "count < ..." vc6.0 thinks this is a template beginning and crashes
      next = BUFSIZE;
      count = BUFSIZE;
      return NS_ERROR_FAILURE;
    }
    count = strm.read(buffer, BUFSIZE);
    next = 0;
    if (count == 0) {
      next = BUFSIZE;
      count = BUFSIZE;
      return NS_ERROR_FAILURE;
    }
  }
  c = buffer[next++];
  return NS_OK;
}

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PUBLIC PRInt32
CKutil_GetLine(nsInputFileStream& strm, nsString& aLine) {

  /* read the line */
  aLine.Truncate();
  char c;
  for (;;) {
    if NS_FAILED(ckutil_getChar(strm, c)) {
      return -1;
    }
    if (c == '\n') {
      break;
    }

    if (c != '\r') {
      aLine.AppendWithConversion(c);
    }
  }
  return 0;
}

PUBLIC char * 
CKutil_ParseURL (const char *url, int parts_requested) {
  char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
  char *atSign, *host, *passwordColon, *gtThan;
  assert(url);
  if(!url) {
    return(CKutil_StrAllocCat(rv, ""));
  }
  colon = PL_strchr(url, ':'); /* returns a const char */
  /* Get the protocol part, not including anything beyond the colon */
  if (parts_requested & GET_PROTOCOL_PART) {
    if(colon) {
      char val = *(colon+1);
      *(colon+1) = '\0';
      CKutil_StrAllocCopy(rv, url);
      *(colon+1) = val;
      /* If the user wants more url info, tack on extra slashes. */
      if( (parts_requested & GET_HOST_PART)
          || (parts_requested & GET_USERNAME_PART)
          || (parts_requested & GET_PASSWORD_PART)) {
        if( *(colon+1) == '/' && *(colon+2) == '/') {
          CKutil_StrAllocCat(rv, "//");
        }
        /* If there's a third slash consider it file:/// and tack on the last slash. */
        if( *(colon+3) == '/' ) {
          CKutil_StrAllocCat(rv, "/");
        }
      }
    }
  }

  /* Get the username if one exists */
  if (parts_requested & GET_USERNAME_PART) {
    if (colon && (*(colon+1) == '/') && (*(colon+2) == '/') && (*(colon+3) != '\0')) {
      if ( (slash = PL_strchr(colon+3, '/')) != NULL) {
        *slash = '\0';
      }
      if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
        *atSign = '\0';
        if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL) {
          *passwordColon = '\0';
        }
        CKutil_StrAllocCat(rv, colon+3);

        /* Get the password if one exists */
        if (parts_requested & GET_PASSWORD_PART) {
          if (passwordColon) {
            CKutil_StrAllocCat(rv, ":");
            CKutil_StrAllocCat(rv, passwordColon+1);
          }
        }
        if (parts_requested & GET_HOST_PART) {
          CKutil_StrAllocCat(rv, "@");
        }
        if (passwordColon) {
          *passwordColon = ':';
        }
        *atSign = '@';
      }
      if (slash) {
        *slash = '/';
      }
    }
  }

  /* Get the host part */
  if (parts_requested & GET_HOST_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/') {
        slash = PL_strchr(colon+3, '/');
        if(slash) {
          *slash = '\0';
        }
        if( (atSign = PL_strchr(colon+3, '@')) != NULL) {
          host = atSign+1;
        } else {
          host = colon+3;
        }
        ques_mark = PL_strchr(host, '?');
        if(ques_mark) {
          *ques_mark = '\0';
        }
        gtThan = PL_strchr(host, '>');
        if (gtThan) {
          *gtThan = '\0';
        }

        /* limit hostnames to within MAX_HOST_NAME_LEN characters to keep from crashing */
        if(PL_strlen(host) > MAX_HOST_NAME_LEN) {
          char * cp;
          char old_char;
          cp = host+MAX_HOST_NAME_LEN;
          old_char = *cp;
          *cp = '\0';
          CKutil_StrAllocCat(rv, host);
          *cp = old_char;
        } else {
          CKutil_StrAllocCat(rv, host);
        }
        if(slash) {
          *slash = '/';
        }
        if(ques_mark) {
          *ques_mark = '?';
        }
        if (gtThan) {
          *gtThan = '>';
        }
      }
    }
  }

  /* Get the path part */
  if (parts_requested & GET_PATH_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/') {
        /* skip host part */
        slash = PL_strchr(colon+3, '/');
      } else {
        /* path is right after the colon */
        slash = colon+1;
      }
      if(slash) {
        ques_mark = PL_strchr(slash, '?');
        hash_mark = PL_strchr(slash, '#');
        if(ques_mark) {
          *ques_mark = '\0';
        }
        if(hash_mark) {
          *hash_mark = '\0';
        }
        CKutil_StrAllocCat(rv, slash);
        if(ques_mark) { 
          *ques_mark = '?';
        }
        if(hash_mark) {
          *hash_mark = '#';
        }
      }
    }
  }

  if(parts_requested & GET_HASH_PART) {
    hash_mark = PL_strchr(url, '#'); /* returns a const char * */
    if(hash_mark) {
      ques_mark = PL_strchr(hash_mark, '?');
      if(ques_mark) {
        *ques_mark = '\0';
      }
      CKutil_StrAllocCat(rv, hash_mark);
      if(ques_mark) {
        *ques_mark = '?';
      }
    }
  }

  if(parts_requested & GET_SEARCH_PART) {
    ques_mark = PL_strchr(url, '?'); /* returns a const char * */
    if(ques_mark) {
      hash_mark = PL_strchr(ques_mark, '#');
      if(hash_mark) {
        *hash_mark = '\0';
      }
      CKutil_StrAllocCat(rv, ques_mark);
      if(hash_mark) {
        *hash_mark = '#';
      }
    }
  }

  /* copy in a null string if nothing was copied in */
  if(!rv) {
    CKutil_StrAllocCopy(rv, "");
  }

  return rv;
}

PRUnichar *
CKutil_Localize(const PRUnichar *genericString) {
  nsresult ret;
  PRUnichar *ptrv = nsnull;
  nsCOMPtr<nsIStringBundleService> pStringService = 
           do_GetService(kStringBundleServiceCID, &ret); 
  if (NS_SUCCEEDED(ret) && (nsnull != pStringService)) {
    nsCOMPtr<nsIStringBundle> bundle;
    ret = pStringService->CreateBundle(LOCALIZATION, getter_AddRefs(bundle));
    if (NS_SUCCEEDED(ret) && bundle) {
      ret = bundle->GetStringFromName(genericString, &ptrv);
      if ( NS_SUCCEEDED(ret) && (ptrv) ) {
        return ptrv;
      }
    }
  }
  return nsCRT::strdup(genericString);
}

PUBLIC nsresult
CKutil_ProfileDirectory(nsFileSpec& dirSpec) {
  nsresult res;
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsIFileSpec> tempSpec;
  
  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(res)) return res;
  
  // TODO: When the calling code can take an nsIFile,
  // this conversion to nsFileSpec can be avoided. 
  nsXPIDLCString pathBuf;
  aFile->GetPath(getter_Copies(pathBuf));
  res = NS_NewFileSpec(getter_AddRefs(tempSpec));
  if (NS_FAILED(res)) return res;
  res = tempSpec->SetNativePath(pathBuf);
  if (NS_FAILED(res)) return res;
  res = tempSpec->GetFileSpec(&dirSpec);
  
  return res;
}

PUBLIC char *
CKutil_StrAllocCopy(char *&destination, const char *source) {
  if(destination) {
    PL_strfree(destination);
    destination = 0;
  }
  destination = PL_strdup(source);
  return destination;
}

PUBLIC char *
CKutil_StrAllocCat(char *&destination, const char *source) {
  if (source && *source) {
    if (destination) {
      int length = PL_strlen (destination);
      destination = (char *) PR_Realloc(destination, length + PL_strlen(source) + 1);
      if (destination == NULL) {
        return(NULL);
      }
      PL_strcpy (destination + length, source);
    } else {
      destination = PL_strdup(source);
    }
  }
  return destination;
}
