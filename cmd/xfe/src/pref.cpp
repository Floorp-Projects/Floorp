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
/* 
   pref.cpp -- stuff shared by preferences dialog and other modules
   Created: Linda Wei <lwei@netscape.com>, 03-Dec-96.
 */



#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "net.h"
#include "xfe.h"
#include "libmocha.h"
#include "java.h"
#include "PrefsDialog.h"

extern "C"
{
	char     *fe_mn_getmailbox(void);
}

extern "C" void fe_installGeneralAppearance()
{
}

extern "C" void fe_installGeneral()
{
}

extern "C" void fe_installGeneralFonts()
{
}

extern "C" void fe_installGeneralColors()
{
    LO_SetUserOverride(	!fe_globalPrefs.use_doc_colors );
}

extern "C" void fe_installGeneralAdvanced()
{
/*	NET_SetCookiePrefs((NET_CookiePrefsEnum)fe_globalPrefs.accept_cookie);*/
	NET_SendEmailAddressAsFTPPassword(fe_globalPrefs.email_anonftp);
}

extern "C" void fe_installGeneralPasswd()
{
}

extern "C" void fe_installGeneralCache()
{
	FE_CacheDir = fe_globalPrefs.cache_dir;
}

extern "C" void fe_installGeneralProxies()
{
	if ((fe_globalPrefs.proxy_mode == 0) ||
		(fe_globalPrefs.proxy_mode == PROXY_STYLE_NONE))
		NET_SelectProxyStyle(PROXY_STYLE_NONE);
	else if (fe_globalPrefs.proxy_mode == PROXY_STYLE_MANUAL)
		NET_SelectProxyStyle(PROXY_STYLE_MANUAL);
	else if (fe_globalPrefs.proxy_mode == PROXY_STYLE_AUTOMATIC) {
		NET_SetProxyServer(PROXY_AUTOCONF_URL, fe_globalPrefs.proxy_url);
		NET_SelectProxyStyle(PROXY_STYLE_AUTOMATIC);
	}
}

extern "C" void fe_installBrowser()
{
	if (fe_globalPrefs.global_history_expiration <= 0)
		GH_SetGlobalHistoryTimeout (-1);
	else
		GH_SetGlobalHistoryTimeout (fe_globalPrefs.global_history_expiration
									* 24 * 60 * 60);
}

extern "C" void fe_installBrowserLang(char *lang)
{
	fe_SetAcceptLanguage(lang);
}

extern "C" void fe_installBrowserAppl()
{
	FE_TempDir = fe_globalPrefs.tmp_dir;
}

#ifdef MOZ_MAIL_NEWS
extern "C" void fe_installMailNews()
{
}

extern "C" void fe_installMailNewsIdentity()
{
	NET_SetMailRelayHost((char *)((fe_globalPrefs.mailhost && *fe_globalPrefs.mailhost)
								  ? fe_globalPrefs.mailhost : "localhost"));
}

extern "C" void fe_installMailNewsComposition()
{
	MIME_ConformToStandard(fe_globalPrefs.qp_p);
}

extern "C" void fe_installMailNewsMserver()
{
	NET_SetPopUsername (fe_globalPrefs.pop3_user_id); 
	MSG_SetBiffStatFile(fe_globalPrefs.use_movemail_p ? fe_mn_getmailbox() :
						(char *)NULL);
}

extern "C" void fe_installMailNewsNserver()
{
	FE_UserNewsHost = fe_globalPrefs.newshost;
	NET_SetNewsHost(FE_UserNewsHost);
	FE_UserNewsRC = fe_globalPrefs.newsrc_directory;
}

extern "C" void fe_installMailNewsAddrBook()
{
}
#endif  // MOZ_MAIL_NEWS

extern "C" void fe_installLangs()
{
}

#ifdef MOZ_LDAP
extern "C" void fe_installMserverMore()
{
	MSG_SetFolderDirectory(fe_mailNewsPrefs, fe_globalPrefs.mail_directory);
	NET_SetPopPassword(fe_globalPrefs.rememberPswd
					   ? fe_globalPrefs.pop3_password : (char *)NULL); 
}
#endif  // MOZ_LDAP

extern "C" void fe_installProxiesView()
{
    char buf[1024];

    // This should be done in libnet with the proxies - malmer
    sprintf(buf, "%s:%d", fe_globalPrefs.socks_host, 
                          fe_globalPrefs.socks_host_port);
	NET_SetSocksHost(buf);
}

extern "C" void fe_installSslConfig()
{
}

extern "C" void fe_installOffline()
{
}

#ifdef MOZ_MAIL_NEWS
extern "C" void fe_installOfflineNews()
{
}
#endif  // MOZ_MAIL_NEWS

extern "C" void fe_installDiskSpace()
{
}

extern "C" void fe_installDiskMore()
{
}

extern "C" void
fe_PrefsDialog(MWContext *context)
{
	Widget             mainw = CONTEXT_WIDGET(context);
	XFE_PrefsDialog   *theDialog = 0;

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
		// TODO: out of memory
	}

	// Open some categories

	theDialog->openCategory(CAT_APPEARANCE);
	theDialog->openCategory(CAT_MAILNEWS);

	// Pop up the preferences dialog

	theDialog->show();
}

int fe_stringPrefToArray(const char *orig_string, char ***array_p)
{
	int    count = 0;
	int    i;
	int    len = 0;
	char **array = 0;
	char  *string = 0;
	char  *ptr;

	len = XP_STRLEN(orig_string);
	if (orig_string && len > 0) {
		string = XP_STRDUP(orig_string);

		// Get the number of entries

		count = 0;
		char *sep = XP_STRCHR(string, ',');
		while (sep) {
			count++;
			sep = XP_STRCHR(sep+1, ',');
		}
		count++;

		array = (char **)XP_CALLOC(count, sizeof(char *));
		i = 0;

		ptr = XP_STRTOK(string, ",");
		while (ptr) {
			array[i] = XP_STRDUP(ptr);
			i++;
			ptr = XP_STRTOK(NULL, ",");
		}
	}

	if (string) XP_FREE(string);
	*array_p = array;
	return count;
}

char *fe_arrayToStringPref(char **array, int count)
{
	char *string = 0;
	int   i;
	char *ptr;

	if (array && (count > 0)) {
		int  len = 0;
		for (i = 0; i < count; i++) {
			len += XP_STRLEN(array[i]);
		}
		len = len + count;
		string = (char *)XP_CALLOC(len, sizeof(char));
		for (ptr= string, i = 0; i < count; i++) {
			int l = XP_STRLEN(array[i]);
			XP_MEMCPY(ptr, array[i], l);
			ptr += l;
			if (i == (count-1))
				*ptr = '\0';
			else
				*ptr = ',';
			ptr++;
		}
	}
	else {
		string = XP_STRDUP("");
	}

	return string;
}
