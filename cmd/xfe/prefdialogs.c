/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   dialogs.c --- Preference dialogs.
   Created: Spence Murray <spence@netscape.com>, 30-Sep-95.
 */


#define PREFS_NET
#define PREFS_SOCKS
#define PREFS_DISK_CACHE
#define PREFS_VERIFY
#define PREFS_CLEAR_CACHE_BUTTONS
#define PREFS_SIG
#define PREFS_NEWS_MAX
#define PREFS_8BIT
/* #define PREFS_EMPTY_TRASH */
/* #define PREFS_QUEUED_DELIVERY */

#include <stdio.h>

#include "mozilla.h"
#include "xlate.h"
#include "xfe.h"
#include "felocale.h"
#include "nslocks.h"
#include "secnav.h"
#include "cert.h"
#include "mozjava.h"
#include "prnetdb.h"
#include "e_kit.h"
#include "pref_helpers.h"

#include "libmocha.h"

#include <Xm/Label.h>
#include <Xm/CascadeBG.h>
#include <Xm/PanedW.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>

#include <Xm/ArrowBG.h>

#ifndef _STDC_
#define _STDC_ 1
#define HACKED_STDC 1
#endif

#include <XmL/Folder.h>

#ifdef HACKED_STDC
#undef HACKED_STDC
#undef _STDC_
#endif

#include "msgcom.h"

#include "libimg.h"             /* Image Library public API. */


/* For sys_errlist and sys_nerr */
#include <sys/errno.h>
#if !defined(__FreeBSD__) && !defined(LINUX_GLIBC_2)
extern char *sys_errlist[];
extern int sys_nerr;
#endif

#include "prprf.h"
#include "libi18n.h"
#include "fonts.h"


/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_UNKNOWN_ERROR;

extern int XFE_FILE_DOES_NOT_EXIST;
extern int XFE_FTP_PROXY_HOST;
#ifdef MOZ_MAIL_NEWS
extern int XFE_GLOBAL_MAILCAP_FILE;
extern int XFE_GLOBAL_MIME_FILE;
#endif
extern int XFE_GOPHER_PROXY_HOST;
extern int XFE_HOST_IS_UNKNOWN;
extern int XFE_HTTPS_PROXY_HOST;
extern int XFE_HTTP_PROXY_HOST;
extern int XFE_NO_PORT_NUMBER;
#ifdef MOZ_MAIL_NEWS
extern int XFE_MAIL_HOST;
extern int XFE_NEWS_HOST;
extern int XFE_NEWS_RC_DIRECTORY;
extern int XFE_PRIVATE_MAILCAP_FILE;
extern int XFE_PRIVATE_MIME_FILE;
#endif
extern int XFE_SOCKS_HOST;
extern int XFE_TEMP_DIRECTORY;
extern int XFE_WAIS_PROXY_HOST;
extern int XFE_WARNING;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_GENERAL;
extern int XFE_PASSWORDS;
extern int XFE_PERSONAL_CERTIFICATES;
extern int XFE_SITE_CERTIFICATES;
extern int XFE_APPEARANCE;
extern int XFE_BOOKMARKS;
extern int XFE_COLORS;
extern int XFE_FONTS;
extern int XFE_APPLICATIONS;
extern int XFE_HELPERS;
extern int XFE_IMAGES;
extern int XFE_LANGUAGES;
extern int XFE_CACHE;
extern int XFE_CONNECTIONS;
extern int XFE_PROXIES;
extern int XFE_PROTOCOLS;
extern int XFE_LANG;
extern int XFE_APPEARANCE;
extern int XFE_COMPOSE_DLG;
extern int XFE_SERVERS;
extern int XFE_IDENTITY;
extern int XFE_ORGANIZATION;
extern int XFE_CHANGE_PASSWORD;
extern int XFE_SET_PASSWORD;
#ifdef MOZ_MAIL_NEWS
extern int XFE_PRIVATE_MIMETYPE_RELOAD;
extern int XFE_PRIVATE_MAILCAP_RELOAD;
extern int XFE_PRIVATE_RELOADED_MIMETYPE;
extern int XFE_PRIVATE_RELOADED_MAILCAP;
#endif

extern void fe_installGeneralAppearance(void);
extern void fe_installGeneralFonts(void);
extern void fe_installGeneralColors(void);
extern void fe_installGeneralAdvanced(void);
extern void fe_installGeneralPasswd(void);
extern void fe_installGeneralCache(void);
extern void fe_installGeneralProxies(void);
extern void fe_installBrowser(void);
extern void fe_installBrowserAppl(void);
#ifdef MOZ_MAIL_NEWS
extern void fe_installMailNews(void);
extern void fe_installMailNewsIdentity(void);
extern void fe_installMailNewsComposition(void);
extern void fe_installMailNewsMserver(void);
extern void fe_installMailNewsNserver(void);
extern void fe_installMailNewsAddrBook(void);
extern void fe_installOfflineNews(void);
extern void fe_installMserverMore(void);
#endif
extern void fe_installProxiesView(void);
extern void fe_installDiskMore(void);
extern void fe_installSslConfig(void);
extern void fe_installOffline(void);
extern void fe_installDiskSpace(void);

/* Preferences
 */

struct fe_prefs_data
{
  MWContext *context;

  Widget shell, form;
  Widget promptDialog;

  /*=== General prefs folder ===*/
  Widget general_form;

  /* Appearance page */
  Widget styles1_selector, styles1_page;
  Widget icons_p, text_p, both_p, tips_p;
  Widget browser_p, mail_p, news_p;
  Widget blank_p, home_p, home_text;
  Widget underline_p;
  Widget expire_days_p, expire_days_text, never_expire_p;

  /* Applications page */
  Widget apps_selector, apps_page;
  Widget tmp_text;
  Widget telnet_text, tn3270_text, rlogin_text, rlogin_user_text;

#if 0
  /* Bookmarks page */
  Widget bookmarks_selector, bookmarks_page;
  Widget book_text;
#endif

  /* Colors page */
  Widget colors_selector, colors_page;

  /* Fonts page */
  Widget fonts_selector, fonts_page;
  Widget prop_family_pulldown, fixed_family_pulldown;
  Widget prop_family_option, fixed_family_option;
  Widget prop_size_pulldown, fixed_size_pulldown;
  Widget prop_size_option, fixed_size_option;
  Widget prop_size_field, fixed_size_field;
  Widget prop_size_toggle, fixed_size_toggle;
  int fonts_changed;

  /* Helpers page */
  Widget helpers_selector, helpers_page;
  int helpers_changed;

  struct fe_prefs_helpers_data *helpers;

  /* Images page */
  Widget images_selector, images_page;
  Widget auto_p, dither_p, closest_p;
  Widget while_loading_p, after_loading_p;

  /* Languages page */
  Widget languages_selector, languages_page;
  Widget lang_reg_avail, lang_reg_pref, lang_reg_text;
  Widget tmpDefaultButton;

  /*=== Mail/News prefs folder ===*/
  Widget mailnews_form;

  /* Appearance page */
  Widget appearance_selector, appearance_page;
  Widget fixed_message_font_p, var_message_font_p;
  Widget cite_plain_p, cite_bold_p, cite_italic_p, cite_bold_italic_p;
  Widget cite_normal_p, cite_bigger_p, cite_smaller_p;
  Widget cite_color_text;
  Widget mail_horiz_p, mail_vert_p, mail_stack_p;
  Widget news_horiz_p, news_vert_p, news_stack_p;
  Widget msgwin_use_mailwin_p, msgwin_use_new_p, msgwin_use_existing_p;

  /* Compose page */
  Widget compose_selector, compose_page;
  Widget eightbit_toggle, qp_toggle;
  Widget deliverAuto_toggle, deliverQ_toggle;
  Widget mMailOutSelf_toggle, mMailOutOther_text;
  Widget nMailOutSelf_toggle, nMailOutOther_text;
  Widget mCopyOut_text;
  Widget nCopyOut_text;
  Widget autoquote_toggle;

  /* Servers page */
  Widget dirs_selector, dirs_page;
  Widget srvr_text, user_text;
  Widget pop_toggle;
  Widget movemail_text;
  Widget external_toggle, builtin_toggle;
  Widget no_limit, msg_limit, limit_text;
  Widget msg_remove, msg_leave;
  Widget check_every, check_never, check_text;
  Widget smtp_text, maildir_text;
  Widget newshost_text, newsrc_text, newsmax_text;

  /* Identity page */
  Widget identity_selector, identity_page;
  Widget user_name_text, user_mail_text, user_org_text, user_sig_text;

  /* Organization page */
  Widget organization_selector, organization_page;
  Widget emptyTrash_toggle, rememberPswd_toggle;
  Widget threadmail_toggle, threadnews_toggle;
  Widget mdate_toggle, mnum_toggle, msubject_toggle, msender_toggle;
  Widget ndate_toggle, nnum_toggle, nsubject_toggle, nsender_toggle;

  /*=== Network prefs folder ===*/
  Widget network_form;

  /* Cache page */
  Widget cache_selector, cache_page;
  Widget memory_text, memory_clear;
  Widget disk_text, disk_dir, disk_clear;
  Widget verify_label, once_p, every_p, expired_p;
  Widget cache_ssl_p;

  /* Nework page */
  Widget network_selector, network_page;
  Widget buf_label, buf_text;
  Widget conn_label, conn_text;

  /* Proxies page */
  Widget proxies_selector, proxies_page;
  Widget no_proxies_p, manual_p, manual_browse;
  Widget auto_proxies_p, proxy_label, proxy_text;
  Widget proxy_reload;

  Widget ftp_text, ftp_port;
  Widget gopher_text, gopher_port;
  Widget http_text, http_port;
  Widget https_text, https_port;
  Widget wais_text, wais_port;
  Widget no_text;
  Widget socks_text, socks_port;

  /* Protocols page */
  Widget protocols_selector, protocols_page;
  Widget cookie_p;	/* Show alert before accepting a cookie */
  Widget anon_ftp_p;	/* Use email address for anon ftp */
  Widget email_form_p;	/* Show alert before submiting a form by email */

  /* Languages page */
  Widget lang_selector, lang_page;
#ifdef JAVA
  Widget java_toggle;
#endif

  Widget javascript_toggle;

  /*=== Security prefs folder ===*/
  Widget security_form;

  /* General Security page */
  Widget sec_general_selector, sec_general_page;
  Widget enter_toggle, leave_toggle, mixed_toggle, submit_toggle;
  Widget ssl2_toggle, ssl3_toggle;

  /* Security Passwords page */
  Widget sec_passwords_selector, sec_passwords_page;
  Widget once_toggle, every_toggle, periodic_toggle;
  Widget periodic_text, change_password;
#ifdef FORTEZZA
  Widget fortezza_toggle, fortezza_timeout;
#endif

  /* Personal Certificates page */
  Widget personal_selector, personal_page;
  Widget pers_list, pers_info, pers_delete_cert, pers_new_cert;
  Widget site_default, pers_label;
  Widget pers_cert_menu;
  char *deleted_user_cert;

  /* Site Certificates page */
  Widget site_selector, site_page;
  Widget all_list, all_edit_cert, all_delete_cert;
  Widget all_label;
  char *deleted_site_cert;
};

void
FE_SetPasswordAskPrefs(MWContext *context, int askPW, int timeout)
{
}

void
FE_SetPasswordEnabled(MWContext *context, PRBool usePW)
{
}


void
fe_MarkHelpersModified(MWContext *context)
{
  /* Mark that the helpers have been modified. We need this
   * to detect if we need to save/reload all these helpers.
   * For doing this, the condition we adopt is
   * (1) User clicked on 'OK' button for the Edit Helpers Dialog
   *	This also covers the "New" case as the same Dialog
   *	is shown for the "New" and "Edit".
   * (2) User deleted a mime/type and confirmed it.
   */
  if (context && CONTEXT_DATA(context)->fep)
    CONTEXT_DATA(context)->fep->helpers_changed = TRUE;
}


const char *multiline_continuator = "\t\t\n";
/*
 * This belongs in a utility module
 *
 * Convert an array of strings
 * to multiline string (embedded new-lines)
 *
 *  returns a string built up from the array
 *
 * The caller must free the array
 */
char *
array_to_multiline_str(char **array, int cnt)
{
  char *mstring, *p;
  int i, str_len;

  /*
   * Handle an empty array
   */
  if (cnt == 0) {
    /* always allocate an array so the caller can free it */
    mstring = strdup("\n");
    return mstring;
  }

  /*
   * Get the length of the multiline string
   */
  str_len = 0;
  for (i=0; i<cnt; i++) {
    if (i != 0)
      str_len += strlen(multiline_continuator);
    str_len += strlen(array[i]);
  }
  str_len += 2; /* space for new-line and terminator */
  mstring = (char *)XP_ALLOC(str_len);

  for (p=mstring, i=0; i<cnt; i++) {
    if (i != 0) {
      strcpy(p, multiline_continuator);
      p += strlen(multiline_continuator);
    }
    strcpy(p, array[i]);
    p += strlen(array[i]);
  }
  /*
   * Note: don't add a final line-feed for multiline
   * as the prefs_write_string() function handles it funny
   */
  return mstring;
}

/*
 * This belongs in a utility module
 *
 * Convert an array of strings
 * to multiline preference (embedded new-lines)
 *
 *  returns a preference value string built up from the array
 *
 * The caller must free the array
 */
char *
array_to_multiline_pref(char **array, int cnt)
{
  char *string;
  string = array_to_multiline_str(array, cnt);
  return string;
}

/*
 * This belongs in a utility module
 *
 * Convert a multiline string (embedded new-lines)
 * to an array of strings
 *  create the array
 *  copies the lines into the array
 *  returns length of the array
 *
 * To free the array:
 *   for (i=0; i<cnt; i++)
 *      XP_FREE(array[i]);
 *   XP_FREE(array);
 */
int
multiline_str_to_array(const char *string, char ***array_p)
{
    char *p, *q, **array;
    int i, cnt, len;
    Boolean at_start_of_line;
    
    /* handle null string */
    if (string == NULL) {
	/* always allocate an array so the caller can free it */
	array = (char **)XP_ALLOC(sizeof(char *));
	array[0] = NULL;
	*array_p = array;
	return 0;
    }

    /* 
     * count the number of lines 
     * (count beginnings so we will count any last line without a '\n')
	 * This supports multibyte text only because '\n' is less than 0x40
     */
    cnt = 0;
    at_start_of_line = True;
    for (p=(char *)string; *p; p++) {
	if (at_start_of_line) {
	    cnt += 1;
	    at_start_of_line = False;
	}
	if (*p == '\n') {
	    at_start_of_line = True;
	}
    }

    /* copy lines into the array */
    array = (char **)XP_ALLOC((cnt+1) * sizeof(char *));
    i = 0;
    len = 0;
    for (p=q=(char *)string; *p; p++) {
	if (*p == '\n') {
	    array[i] = (char *)XP_ALLOC(len+1);
	    strncpy(array[i], q, len);
	    /* add a string terminator */
	    array[i][len] = '\0';
	    i += 1;
	    len = 0;
	    q = p + 1;
	}
	else
	    len += 1;
    }
    if (len) { /* include ending chars with no newline */
	array[i] = (char *)XP_ALLOC(len+1);
	strncpy(array[i], q, len);
	/* add a string terminator */
	array[i][len] = '\0';
    }

    *array_p = array;
    return cnt;
}

/*
 * This belongs in a utility module
 *
 * Convert a multiline preference (embedded new-lines)
 * to an array of strings
 *  create the array
 *  copies the lines into the array
 *  remove leading/trailing white space
 *  remove blank lines
 *  returns length of the array
 *
 * To free the array:
 *   for (i=0; i<cnt; i++)
 *      XP_FREE(array[i]);
 *   XP_FREE(array);
 */
int
multiline_pref_to_array(const char *string, char ***array_p)
{
  int i, j, cnt;
  char **array;

  /*
   * Convert the multiline string to an array
   * (with leading/trailing white space and blank lines)
   */
  cnt = multiline_str_to_array(string, array_p);

  /*
   * trim any leading/training white space
   */
  array = *array_p;
  for (i=0; i<cnt; i++) {
    char *tmp;
    tmp = XP_StripLine(array[i]);
    /* we can't lose the malloc address else free will fail */
    /* so if there was white space at the beginning we make a copy */
    if (tmp != array[i]) {
	char *tmp2 = array[i];
      	array[i] = strdup(tmp);
      	XP_FREE(tmp2);
    }
  }

  /*
   * remove any blank lines
   */
  for (i=0; i<cnt; i++) {
    if (array[i][0] == '\0') {
	XP_FREE(array[i]);
	for (j=i+1; j<cnt; j++)
	    array[j-1] = array[j];
	cnt -= 1;
    }
  }

  return cnt;
}

IL_DitherMode
fe_pref_string_to_dither_mode (char *s)
{
  if (!XP_STRCASECMP(s, "True") || !XP_STRCASECMP(s, "Dither"))
    return IL_Dither;
  else if (!XP_STRCASECMP(s, "False") || !XP_STRCASECMP(s, "ClosestColor"))
    return IL_ClosestColor;

  /* In the absence of a reasonable value, dithering is auto-selected */
  return IL_Auto;
}

#ifdef MOZ_MAIL_NEWS
void
fe_SetMailNewsSortBehavior(MWContext* context, XP_Bool thread, int sortcode)
{
  XP_ASSERT(context->type == MWContextMail || context->type == MWContextNews);
  if (context->type == MWContextMail || context->type == MWContextNews) {
    if (thread) {
      MSG_SetToggleStatus(ACTIVE_MAILTAB(context)->threadpane, MSG_SortByThread, NULL, 0,
			  thread ? MSG_Checked : MSG_Unchecked);
    } else {
      switch (sortcode) {
      case 0:
	MSG_Command(ACTIVE_MAILTAB(context)->threadpane, MSG_SortByDate, NULL, 0);
	break;
      case 1:
	MSG_Command(ACTIVE_MAILTAB(context)->threadpane, MSG_SortByMessageNumber, NULL, 0);
	break;
      case 2:
	MSG_Command(ACTIVE_MAILTAB(context)->threadpane, MSG_SortBySubject, NULL, 0);
	break;
      case 3:
	MSG_Command(ACTIVE_MAILTAB(context)->threadpane, MSG_SortBySender, NULL, 0);
	break;
      }
    }
  }
}
#endif

void
fe_InstallPreferences (MWContext *context)
{
	/* This function is called from main() */

	fe_installGeneralAppearance();
	fe_installGeneralFonts();
	fe_installGeneralColors();
	fe_installGeneralAdvanced();
	fe_installGeneralPasswd();
	fe_installGeneralCache();
	fe_installGeneralProxies();
	fe_installDiskSpace();

	fe_installBrowser();
	fe_installBrowserAppl();

#ifdef MOZ_MAIL_NEWS
	fe_installMailNews();
	fe_installMailNewsIdentity();
	fe_installMailNewsComposition();
	fe_installMailNewsMserver();
	fe_installMailNewsNserver();
	fe_installMailNewsAddrBook();
#endif

	fe_installOffline();
#ifdef MOZ_MAIL_NEWS
	fe_installOfflineNews();
	fe_installMserverMore();
#endif

	fe_installProxiesView();
	fe_installSslConfig();
	fe_installDiskMore();

	/* CACHE
	 */

	/* spider begin */
        FE_SARCacheDir = fe_globalPrefs.sar_cache_dir ;
        /* spider end */
	NET_DontDiskCacheSSL(!fe_globalPrefs.cache_ssl_p);

	/* bookmark_file
	 */

	FE_GlobalHist = fe_globalPrefs.history_file;

	/* NETWORK
	 */

	NET_ChangeMaxNumberOfConnections (50);
	NET_ChangeMaxNumberOfConnectionsPerContext (fe_globalPrefs.max_connections);
	/* The unit for tcp buffer size is changed from kbyes to btyes */
	NET_ChangeSocketBufferSize (fe_globalPrefs.network_buffer_size);

	/* NEWS
	 */

#ifdef MOZ_MAIL_NEWS
	NET_SetNumberOfNewsArticlesInListing (fe_globalPrefs.news_max_articles);
	NET_SetCacheXOVER (fe_globalPrefs.news_cache_xover);
#endif

	/* PROTOCOLS
	 */

#ifdef MOZ_MAIL_NEWS
	NET_WarnOnMailtoPost(fe_globalPrefs.email_submit);
#endif

#ifdef FORTEZZA
	if (fe_globalPrefs.fortezza_toggle) {
		int timeout = fe_globalPrefs.fortezza_timeout;
		/* '0' minute timeout because 1 minute... still pretty darn fast */
		FortezzaSetTimeout(timeout ? timeout : 1);
	} else {
		FortezzaSetTimeout(0);
	}
#endif
}

void
fe_GeneralPrefsDialog (MWContext *context)
{
}

void
fe_MailNewsPrefsDialog (MWContext *context)
{
}

void
fe_NetworkPrefsDialog (MWContext *context)
{
}

void
fe_SecurityPrefsDialog (MWContext *context)
{
}

/* spider begin */
void
fe_VerifyDiskCache (MWContext *context)
{

#ifdef PREFS_DISK_CACHE

  static Boolean done = False;
  if (done) return;
  done = True;

  if (fe_globalPrefs.disk_cache_size <= 0)
    return;

  fe_VerifyDiskCacheExistence(context, fe_globalPrefs.cache_dir) ;

#endif /* PREFS_DISK_CACHE */

#ifdef PREFS_OLD_WAY
#ifdef PREFS_DISK_CACHE
  struct stat st;
  char file [1024];
  char message [4000];
  static Boolean done = False;
  if (done) return;
  done = True;

  if (fe_globalPrefs.disk_cache_size <= 0)
    return;

  strcpy (file, fe_globalPrefs.cache_dir);
  if (file [strlen (file) - 1] == '/')
    file [strlen (file) - 1] = 0;

  if (stat (file, &st))
    {
      /* Doesn't exist - try to create it. */
      if (mkdir (file, 0700))
	{
	  /* Failed. */
	  char *error = ((errno >= 0 && errno < sys_nerr)
			 ? sys_errlist [errno]
			 : XP_GetString( XFE_UNKNOWN_ERROR));
	  PR_snprintf (message, sizeof (message),
		    fe_globalData.create_cache_dir_message,
		    file, error);
	}
      else
	{
	  /* Suceeded. */
	  PR_snprintf (message, sizeof (message),
			fe_globalData.created_cache_dir_message, file);
	}
    }
  else if (! (st.st_mode & S_IFDIR))
    {
      /* Exists but isn't a directory. */
      PR_snprintf (message, sizeof (message),
		    fe_globalData.cache_not_dir_message, file);
    }
  else
    {
      /* The disk cache is ok! */
      *message = 0;
    }

  if (*message)
    {
      PR_snprintf(message + strlen (message), sizeof (message),
	      fe_globalData.cache_suffix_message,
	      fe_globalPrefs.disk_cache_size, fe_progclass);
      FE_Alert (context, message);
    }
#endif /* PREFS_DISK_CACHE */
#endif /* PREFS_OLD_WAY */
}

void
fe_VerifySARDiskCache (MWContext *context)
{

#ifdef PREFS_DISK_CACHE

  fe_VerifyDiskCacheExistence(context, fe_globalPrefs.sar_cache_dir) ;

#endif /* PREFS_DISK_CACHE */
}

void
fe_VerifyDiskCacheExistence (MWContext *context, char * cache_directory)
{

#ifdef PREFS_DISK_CACHE
  struct stat st;
  char file [1024];
  char message [4000];

  if (!cache_directory)
    return ;

  strcpy (file, cache_directory);
  if (file [strlen (file) - 1] == '/')
    file [strlen (file) - 1] = 0;

  if (stat (file, &st))
    {
      /* Doesn't exist - try to create it. */
      if (mkdir (file, 0700))
	{
	  /* Failed. */
	  char *error = ((errno >= 0 && errno < sys_nerr)
			 ? sys_errlist [errno]
			 : XP_GetString( XFE_UNKNOWN_ERROR));
	  PR_snprintf (message, sizeof (message),
		    fe_globalData.create_cache_dir_message,
		    file, error);
	}
      else
	{
	  /* Suceeded. */
	  PR_snprintf (message, sizeof (message),
			fe_globalData.created_cache_dir_message, file);
	}
    }
  else if (! (st.st_mode & S_IFDIR))
    {
      /* Exists but isn't a directory. */
      PR_snprintf (message, sizeof (message),
		    fe_globalData.cache_not_dir_message, file);
    }
  else
    {
      /* The disk cache is ok! */
      *message = 0;
    }

  if (*message)
    {
      PR_snprintf(message + strlen (message), sizeof (message),
	      fe_globalData.cache_suffix_message,
	      fe_globalPrefs.disk_cache_size, fe_progclass);
      FE_Alert (context, message);
    }
#endif /* PREFS_DISK_CACHE */
}

/* spider end */

