/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsJSSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "plstr.h"
#include "nspr.h"
#include "prmem.h"

static NS_DEFINE_IID(kIScriptSecurityManagerIID, NS_ISCRIPTSECURITYMANAGER_IID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

nsJSSecurityManager::nsJSSecurityManager()
{
  NS_INIT_REFCNT();
  mPrefs = nsnull;
}

nsJSSecurityManager::~nsJSSecurityManager()
{
  nsServiceManager::ReleaseService(kPrefServiceCID, mPrefs);
}

NS_IMPL_ISUPPORTS(nsJSSecurityManager, kIScriptSecurityManagerIID)

NS_IMETHODIMP
nsJSSecurityManager::Init() 
{
  return nsServiceManager::GetService(kPrefServiceCID, nsIPref::GetIID(), (nsISupports**)&mPrefs);
}

NS_IMETHODIMP
nsJSSecurityManager::CheckScriptAccess(nsIScriptContext* aContext, 
                                       void* aObj, 
                                       const char* aProp, 
                                       PRBool* aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

char *
nsJSSecurityManager::AddSecPolicyPrefix(JSContext *cx, char *pref_str)
{
  const char *subjectOrigin = "";//GetSubjectOriginURL(cx);
  char *policy_str;
  char *retval = 0;

  if ((policy_str = GetSitePolicy(subjectOrigin)) == 0) {
  /* No site-specific policy.  Get global policy name. */

	if (NS_OK != mPrefs->CopyCharPref("javascript.security_policy", (char**)&policy_str))
	  policy_str = 0;
  }
  if (policy_str != 0 && PL_strcasecmp(policy_str, "default") != 0) {
    retval = PR_sprintf_append(NULL, "js_security.%s.%s", policy_str, pref_str);
  }

  PR_FREEIF(policy_str);
  return retval;
}

/* Get the site-specific policy associated with object origin org. */
char *
nsJSSecurityManager::GetSitePolicy(const char *org)
{
  char *sitepol;
  char *sp;
  char *nextsp;
  char *orghost = 0;
  char *retval = 0;
  char *prot;
  int splen;
  char *bar;
  char *end;
  char *match = 0;
  int matlen;

  if (NS_OK != mPrefs->CopyCharPref("js_security.site_policy", &sitepol)) {
    return 0;
  }

  /* Site policy comprises text of the form
   *	site1-policy,site2-policy,...,siteNpolicy
   * where each site-policy is
   *	site|policy
   * and policy is presumed to be one of strict/moderate/default
   * site may be either a URL or a hostname.  In the former case
   * we do a prefix match with the origin URL; in the latter case
   * we just compare hosts.
   */

  /* Process entry by entry.  Take longest match, to account for
   * cases like:
   *	http://host/|moderate,http://host/dir/|strict
   */
  for (sp = sitepol; sp != 0; sp = nextsp) {
    if ((nextsp = strchr(sp, ',')) != 0) {
	    *nextsp++ = '\0';
    }

    if ((bar = strchr(sp, '|')) == 0) {
	    continue;			/* no | for this entry */
    }
	  *bar = '\0';

	  /* Isolate host, then policy. */
	  sp += strspn(sp, " ");	/* skip leading spaces */
	  end = sp + strcspn(sp, " |"); /* skip up to space or | */
	  *end = '\0';
    if ((splen = end-sp) == 0) {
	    continue;			/* no URL or hostname */
    }

	  /* Check whether this is long enough. */
    if (match != 0 && matlen >= splen) {
	    continue;			/* Nope.  New shorter than old. */
    }

	  /* Check which case, URL or hostname, we're dealing with. */
    if ((prot = ParseURL(sp, GET_PROTOCOL_PART)) != 0 && *prot != '\0') {
	    /* URL case.  Do prefix match, make sure we're at proper boundaries. */
	    if (PL_strncmp(org, sp, splen) != 0 || (org[splen] != '\0'	/* exact match */
		      && sp[splen-1] != '/'	/* site policy ends with / */
	        && org[splen] != '/'	/* site policy doesn't, but org does */
		      )) {
		    PR_Free(prot);
		    continue;			/* no match */
	    }
  	}
	  else {
	    /* Host-only case. */
	    PR_FREEIF(prot);

      if (orghost == 0 && (orghost = ParseURL(org, GET_HOST_PART)) == 0) {
		    return 0;			/* out of mem */
      }
      if (PL_strcasecmp(orghost, sp) != 0) {
		    continue;			/* no match */
      }
	  }
	  /* Had a match.  Remember policy and length of host/URL match. */
	  match = bar;
	  matlen = splen;
  }

  if (match != 0) {
	  /* Longest hostname or URL match.  Get policy.
	  ** match points to |.
	  ** Skip spaces after | and after policy name.
	  */
	  ++match;
	  sp = match + strspn(match, " ");
	  end = sp + strcspn(sp, " ");
	  *end = '\0';
	  if (sp != end) {
	    retval = PL_strdup(sp);
	  }
  }

  PR_FREEIF(orghost);
  PR_FREEIF(sitepol);
  return retval;
}

PRInt32 
nsJSSecurityManager::CheckForPrivilege(JSContext *cx, char *prop_name, int priv_code)
{
  int i;
  char *markp;
  char *tmp_prop_name;
  char prop_val_buf[128];
  int prop_val_len = sizeof(prop_val_buf); 
  JSBool priv = JS_TRUE;

  if(prop_name == NULL) {
    return NS_SECURITY_FLAG_NO_ACCESS;
  }

  tmp_prop_name = AddSecPolicyPrefix(cx, prop_name);
  if(tmp_prop_name == NULL) {
    return JS_TRUE;
  }

  PRInt32 secLevel;

  if (NS_OK == mPrefs->GetIntPref(tmp_prop_name, &secLevel)) {
    return secLevel;
  }

  return NS_SECURITY_FLAG_NO_ACCESS;
}

static const char* continue_on_violation = "continue_on_access_violation";

JSBool
nsJSSecurityManager::ContinueOnViolation(JSContext *cx, int pref_code)
{
  PRBool cont;

  char *pref_str;

  pref_str = (char*)continue_on_violation;
  pref_str = AddSecPolicyPrefix(cx, pref_str);
  if (pref_str == NULL) {
    return JS_TRUE;
  }

  mPrefs->GetBoolPref(pref_str, &cont);

  if(cont) {
	  return JS_TRUE;
  }

  return JS_FALSE;
}

/* Check named property for access; if fail, check for
 * permission to continue from violation.
 * Arguments:
 *	priv_code	privilege:  LM_PRIV_READONLY or LM_PRIV_READWRITE
 *	pref_code	prefix for continuation
 *			(arg. to lm_ContinueOnViolation)
 * Returns:
 *	LM_PRIV_OK	if access okay
 *	JS_TRUE		if access denied, but continuation (interpretation) okay
 *	JS_FALSE	if access denied, continuation denied
 */
JSBool
nsJSSecurityManager::CheckForPrivilegeContinue(JSContext *cx, char *prop_name, int priv_code, int pref_code)
{
  if (CheckForPrivilege(cx, prop_name, priv_code) == JS_TRUE) {
	  return JS_TRUE;
  }

  //JS_ReportError(cx, "Access denied: Cannot %s %s",
	  //             priv_code == LM_PRIV_READONLY ? "read" : "write", prop_name);

  return ContinueOnViolation(cx, pref_code);
}

//def'ing ACL stuff for the moment.
#if 0
/* 
static JSObject *
getObjectDocument(JSContext *cx, JSObject *container)
{
    while(container) {

	if(JS_InstanceOf(cx, container, &lm_layer_class, 0)) {
            return lm_GetLayerDocument(cx, container);
	} else if (JS_InstanceOf(cx, container, &lm_window_class, 0)) {
	    MochaDecoder *decoder = JS_GetInstancePrivate(cx, container,
						       &lm_window_class, NULL);

	    return decoder? decoder->document: NULL;
	}

	container = JS_GetParent(cx, container);
    }
    return NULL;
}

// Get ACL for obj.  If there's an explicit ACL, return it.
// Otherwise, return the implicit ACL.
char *
lm_GetObjectACL(JSContext *cx, JSObject *obj)
{
    JSDocument *doc;
    char *acl;
    char *doc_str;
    char *slash;

    fprintf(stderr, "in lm_GetObjectACL\n");

    // Get/check explicit value.
    doc = JS_GetPrivate(cx, getObjectDocument(cx, obj));
    doc_str = lm_GetDocACLName(doc);
    fprintf(stderr, "*#*#*#*#* doc = %s #*#*#*#*\n", doc_str);
    if (PREF_CopyCharPref(doc_str, &acl) != PREF_OK)
	acl = 0;
    XP_FREEIF(doc_str);

    if (acl == 0) {
	// Get the default value
	acl = lm_GetObjectOriginURL(cx, obj);
	if (acl == 0 || XP_STRCMP(acl, lm_unknown_origin_str) == 0
#if 0	// ???
	|| (PL_strlen(origin) <= 1)
#else	// there really seem to be such!
	|| acl[0] == '\0'
#endif
	)
            return NULL;

	fprintf(stderr, "--> OOORRRIIIGGGIIINNN %s\n", acl);
	fflush(stderr);

	// Default ACL for object is protocol://host[:port].
	acl = NET_ParseURL(acl, GET_PROTOCOL_PART | GET_HOST_PART);
    }

    fprintf(stderr, "ACL -> %s\n", acl);
    fflush(stderr);

    return acl;
}

static JSBool
CheckTrustedHosts(JSContext *cx, char *subjectOriginURL, char *objectOriginURL)
{
    int i, j, k;
    int acl_len;
    const char *subjectOriginDomain, *objectOriginDomain;
    char *hostlist;
    const char objectHostList[1024];
    char substr[1024];
    JSBool no_match = JS_FALSE;
    
    // Get the domain of the subject origin

    subjectOriginDomain = getCanonicalizedOrigin(cx, subjectOriginURL);

    fprintf(stderr, "Subject Origin Domain - %s, ", subjectOriginDomain);

    objectOriginDomain = getCanonicalizedOrigin(cx, objectOriginURL);

    fprintf(stderr, "Object Origin Domain - %s\n", objectOriginDomain);
    
    hostlist = AddSecPolicyPrefix(cx, "trusted_domain_pair");

    if(hostlist[0] == '\0') {
        return JS_TRUE;
    }
  
    acl_len = sizeof(objectHostList);
    XP_BZERO(objectHostList, acl_len);
    PREF_GetCharPref(hostlist, objectHostList, &acl_len);

    acl_len = XP_STRLEN(objectHostList);
    j = k = 0;
    while(j < acl_len) {
        k++;
        XP_BZERO(substr, 1024);

        for(i = 0; (j < acl_len) && (objectHostList[j] != ','); j++) {
            if(!isspace(objectHostList[j])) {
                substr[i++] = objectHostList[j];
            }
        }

        if(k % 2) {
            if(XP_STRCMP(substr, subjectOriginDomain)) {
                no_match = JS_FALSE;
                continue;
            }
        } else if(no_match) {
           continue;
        }
           
        // Remove trailing '/'
        if(substr[i-1] == '/')
            i--;

        substr[i] = '\0';

        if(XP_STRCMP(objectOriginDomain, substr) == 0)
            return JS_TRUE;
        

        while(j < acl_len && (objectHostList[j] == ',' || isspace(objectHostList[j])))
            j++;
        
    }

    return JS_FALSE;
}

JSBool
lm_CheckACL(JSContext *cx, JSObject *obj, JSTarget target)
{
    const char *sOrigin;
    const char *objectACL;
    const char *oOrigin;
    char *acl;
    char *subj;
    char *subjhost = 0;
    char *slash;
    char *aclmem, *nextmem;
    JSBool allow = JS_FALSE;
    
    fprintf(stderr, "in lm_CheckACL\n");

    // Get the ACL associated with the object(document).
    if ((objectACL = lm_GetObjectACL(cx, obj)) == 0)
	return JS_TRUE;			// unrestricted access (??)

    // Get the subject origin.
    sOrigin = lm_GetSubjectOriginURL(cx);
    if (NET_URL_Type(sOrigin) == WYSIWYG_TYPE_URL)
        sOrigin = LM_SkipWysiwygURLPrefix(sOrigin);

    // Object is always on its own ACL. 
    oOrigin = lm_GetObjectOriginURL(cx, obj);
    if (XP_STRCMP(sOrigin, oOrigin) == 0) {
	allow = JS_TRUE;
	goto done;
    }

    fprintf(stderr, "-- subj: %s\n-- obj ACL: %s\n", sOrigin, objectACL);

    // Now, check whether sOrigin is part of objectACL.
    // Walk through space-separated elements of ACL, doing prefix match
    // of ACL member against sOrigin.

    // First strip last part of sOrigin path.
    subj = NET_ParseURL(sOrigin, GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
    if ((slash = XP_STRRCHR(subj, '/')) != 0)
	*slash = '\0';
    acl = XP_STRDUP(objectACL);

    // Walk through the ACL to see whether the subject host/URL is in
    //  the ACL.  There are two styles of check:
    //	- if the ACL member is a pure host; check for match against
    //	  host in subject URL.
    //	- otherwise, do pure prefix string match.
    // Determine whether the member is a pure host by whether it does or
    // does not have a protocol specified.
    //
    for (aclmem = acl; aclmem != 0; aclmem = nextmem) {
	int memlen;
	char *prot;

	// Chop up list, trim leading spaces.
	if ((nextmem = XP_STRCHR(aclmem, ' ')) != 0)
	    *nextmem++ = '\0';
	aclmem += strspn(aclmem, " ");
	memlen = XP_STRLEN(aclmem);

	// Decide which case we've got.
        if ((prot = NET_ParseURL(aclmem, GET_PROTOCOL_PART)) != 0 && *prot != '\0') {
	    // URL case.  Do prefix match, make sure we're at proper boundaries. 
	    if (   XP_STRNCMP(subj, aclmem, memlen) == 0
	        && (   subj[memlen] == '\0'	// exact match
		    || aclmem[memlen-1] == '/'	// ACL ends with /
	            || subj[memlen] == '/'	// ACL doesn't, but subj starts
		   )
		) {
		allow = JS_TRUE;
		XP_FREE(prot);
		break;
	    }
	}
	else {
	    // Host-only case.
	    XP_FREEIF(prot);
	    if (subjhost == 0)
		subjhost = NET_ParseURL(sOrigin, GET_HOST_PART);
	    if ((allow = (XP_STRCASECMP(subjhost, aclmem) == 0)) != 0)
		break;
	}
    }

    XP_FREEIF(subjhost);

    if(allow == JS_FALSE) {
	char *err_mesg =
	    PR_smprintf("Access disallowed from scripts at %s to documents at %s",
	    	sOrigin, lm_GetObjectOriginURL(cx,obj));
	JS_ReportError(cx, err_mesg);
	XP_FREEIF(err_mesg);
    }

    XP_FREEIF(subj);
    XP_FREEIF(acl);

done:;
    fprintf(stderr, "-- returns %s\n", allow == JS_TRUE ? "true" : "false");
    return allow;
}

char *
lm_NotifyUserAboutACLExpansion(JSContext *cx)
{
    int i, j;
    int uhl_len;
    const char *subjectOriginDomain;
    char substr[1024];
    char *pref;
    char untrustedHostsBuf[1024];
    
    pref = AddSecPolicyPrefix(cx, "untrusted_host");

    if(pref == NULL) 
        return NULL;

    uhl_len = sizeof(untrustedHostsBuf);
    XP_BZERO(untrustedHostsBuf, uhl_len);
    PREF_GetCharPref(pref, untrustedHostsBuf, &uhl_len);
    XP_FREEIF(pref);

    if(!untrustedHostsBuf[0])
        return NULL;

    // Get the domain of the subject origin

    subjectOriginDomain = getCanonicalizedOrigin(cx, lm_GetSubjectOriginURL(cx));

    fprintf(stderr, "subjectDomain - %s, \n", subjectOriginDomain);

    // Now, check whether subjectOriginDomain
    // is part of untrustedHostsBuf
    uhl_len = PL_strlen(untrustedHostsBuf);
    j = 0;
    while(j < uhl_len) {
        XP_BZERO(substr, 1024);

        for(i = 0; (j < uhl_len) && (untrustedHostsBuf[j] != ','); j++) {
            if(!isspace(untrustedHostsBuf[j])) {
                substr[i++] = untrustedHostsBuf[j];
            }
        }

        // Remove trailing '/'
        if(substr[i-1] == '/')
            i--;

        substr[i] = '\0';

        if(XP_STRCMP(substr, subjectOriginDomain) == 0) {
            char *message_str = PR_smprintf("Script from untrusted domain: %s trying to expand ACL\n Allow?", subjectOriginDomain);
            return message_str;
        }

        fprintf(stderr, "lm_NotifyUserAboutACLExpansion: substr - %s, \n", substr);

        while((j < uhl_len) && 
              (untrustedHostsBuf[j] == ',' || isspace(untrustedHostsBuf[j]))) {
            j++;
        }
        
    }

    return NULL;
}

JSBool
lm_CheckPrivateTag(JSContext *cx, JSObject *obj, jsval id)
{
    char *name;
    const char *subjectOrigin, *objectOrigin;
    JSObject *parent;

    if(!JSVAL_IS_INT(id))
        return JS_FALSE;

    fprintf(stderr, "Inside lm_CheckPrivateTag, id %d\n", JSVAL_TO_INT(id));

    // May be in a layer loaded from a different origin.
    subjectOrigin = lm_GetSubjectOriginURL(cx);
    objectOrigin = lm_GetObjectOriginURL(cx, obj);

    // If the subjectURL and objectURL don't match, stop here
    if (   subjectOrigin != 0
        && objectOrigin != 0
	&& XP_STRCMP(subjectOrigin, objectOrigin) == 0
	&& XP_STRCMP(subjectOrigin, lm_unknown_origin_str) != 0
    )
        return JS_FALSE;

    // Check for private tag by examining object, then its ancestors.
    name = lm_GetPrivateName_slot(JSVAL_TO_INT(id));

    for (parent = obj; parent != 0; parent = JS_GetParent(cx, obj)) {
	 jsval val;

	fprintf(stderr, "(((( %s ))))\n", name);

	if (   JS_GetProperty(cx, obj, name, &val)
	    && val != JSVAL_VOID
	    && JSVAL_TO_BOOLEAN(val) == JS_TRUE
	) {
	    JS_ReportError(cx, "Cannot access private property");
	    return JS_TRUE;
	}

        // Now check for private tag on composite objects
	obj = parent;
	XP_FREE(name);
	name = lm_GetPrivateName_obj(obj);
    }
    XP_FREE(name);
    return JS_FALSE;
}
*/
#endif //def'ing out ACL code.

#define PMAXHOSTNAMELEN 64

//XXX This is only here until I have a new Netlib equivalent!!!
char * 
nsJSSecurityManager::ParseURL (const char *url, int parts_requested)
{
	char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
	char *atSign, *host, *passwordColon, *gtThan;

  if(!url) {
		return(SACat(rv, ""));
  }
	colon = PL_strchr(url, ':'); /* returns a const char */

	/* Get the protocol part, not including anything beyond the colon */
	if (parts_requested & GET_PROTOCOL_PART) {
		if(colon) {
			char val = *(colon+1);
			*(colon+1) = '\0';
			SACopy(rv, url);
			*(colon+1) = val;

			/* If the user wants more url info, tack on extra slashes. */
			if ((parts_requested & GET_HOST_PART) || 
          (parts_requested & GET_USERNAME_PART) || 
          (parts_requested & GET_PASSWORD_PART)) {
        if( *(colon+1) == '/' && *(colon+2) == '/') {
          SACat(rv, "//");
        }
				/* If there's a third slash consider it file:/// and tack on the last slash. */
        if ( *(colon+3) == '/' ) {
					SACat(rv, "/");
        }
			}
		}
	}

	/* Get the username if one exists */
	if (parts_requested & GET_USERNAME_PART) {
		if (colon && 
        (*(colon+1) == '/') && 
        (*(colon+2) == '/') && 
        (*(colon+3) != '\0')) {

      if ( (slash = PL_strchr(colon+3, '/')) != NULL) {
				*slash = '\0';
      }
			if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
				*atSign = '\0';
        if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL) {
					*passwordColon = '\0';
        }
				SACat(rv, colon+3);

				/* Get the password if one exists */
				if (parts_requested & GET_PASSWORD_PART) {
					if (passwordColon) {
						SACat(rv, ":");
						SACat(rv, passwordColon+1);
					}
				}
        if (parts_requested & GET_HOST_PART) {
					SACat(rv, "@");
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
        }
        else {
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

        /* limit hostnames to within PMAXHOSTNAMELEN characters to keep
         * from crashing
         */
        if(PL_strlen(host) > PMAXHOSTNAMELEN) {
          char * cp;
          char old_char;

          cp = host + PMAXHOSTNAMELEN;
          old_char = *cp;

          *cp = '\0';

          SACat(rv, host);

          *cp = old_char;
        }
			  else {
				  SACat(rv, host);
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
			}
			else {
				/* path is right after the colon
				 */
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

        SACat(rv, slash);

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

      SACat(rv, hash_mark);

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

      SACat(rv, ques_mark);

      if(hash_mark) {
          *hash_mark = '#';
      }
    }
  }

	/* copy in a null string if nothing was copied in
	 */
  if(!rv) {
	  SACopy(rv, "");
  }
    
  return rv;
}

char * 
nsJSSecurityManager::SACopy (char *destination, const char *source)
{
	if(destination) {
	  PR_Free(destination);
		destination = 0;
	}
  if (!source)	{
    destination = NULL;
	}
  else {
    destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (destination == NULL) 
 	    return(NULL);

    PL_strcpy (destination, source);
  }
  return destination;
}


char *
nsJSSecurityManager::SACat (char *destination, const char *source)
{
    if (source && *source)
      {
        if (destination)
          {
            int length = PL_strlen (destination);
            destination = (char *) PR_Realloc (destination, length + PL_strlen(source) + 1);
            if (destination == NULL)
            return(NULL);

            PL_strcpy (destination + length, source);
          }
        else
          {
            destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (destination == NULL)
                return(NULL);

             PL_strcpy (destination, source);
          }
      }
    return destination;
}



extern "C" NS_DOM nsresult NS_NewScriptSecurityManager(nsIScriptSecurityManager ** aInstancePtrResult)
{
  nsIScriptSecurityManager* it = new nsJSSecurityManager();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult ret = it->QueryInterface(kIScriptSecurityManagerIID, (void **) aInstancePtrResult);
  
  if (NS_FAILED(ret)) {
    return ret;
  }

  ret = it->Init();

  if (NS_FAILED(ret)) {
    NS_RELEASE(*aInstancePtrResult);
  }

  return ret;
}

