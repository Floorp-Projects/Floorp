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
   Netcaster.cpp -- utilities to open Netcaster
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */




#include "mozjava.h"
#include "xfe2_extern.h"
#include "VerReg.h"
#include "libmocha.h"
#include "xpgetstr.h"

#include "Netcaster.h"

// This is way-less expensive than including allxpstr.h
extern int XP_ALERT_NETCASTER_NO_JS;
extern int XP_ALERT_CANT_RUN_NETCASTER;

/*
 * rodt: Defined in ns/cmd/xfe/mozilla.c.  The rest of the world
 * also does this cheesy declaration.
 */
extern "C" void fe_GetProgramDirectory(char * path, int length);

static char* netcasterWindowName          = "Netcaster_SelectorTab";
static char* netcasterTabHtmlRegistryNode = "Netcaster/tab.htm";
static char* netcasterTabHtmlPath         = "netcast/tab.htm";

/*
 * These statics were moved outside of functions where they are
 * used because of potential problems on HP (rodt, djw, dora).
 * They are for private use within their respective functions!
 */
static char * private_xfe_netcaster_path = 0;
static char * private_xfe_netcaster_url = 0;



/****************************************
 *
 */

MWContext *
FE_IsNetcasterRunning(void)

/*
 * returns:
 *	A pointer to an MWContext if Netcaster is running.
 *	Null if Netcaster is not running.
 *
 ****************************************/
{
  MWContext * result = 0;

  if (fe_IsNetcasterInstalled())
	  result = XP_FindNamedContextInList(NULL, netcasterWindowName);

  return result;
}


/****************************************
 */

static XP_Bool
xfe_path_exists(const char * path)

/*
 * returns:
 *	TRUE if the path exists on disk
 *	FALSE otherwise
 *
 ****************************************/
{
  XP_Bool     result = FALSE;
  int         code;
  struct stat sbuf;
  const       STAT_SUCCESS = 0;

  if (path)
	{
	  /* rodt: didn't use XP_Stat() because it does a lot of apparently
	   * unnecessary work, is undocumented and also is implemented using stat().
	   */
	  code = stat(path, &sbuf);
	  if (code == STAT_SUCCESS)
		result = TRUE;
	}

  return result;
}


/****************************************
 *
 */

static int
xfe_last_character(const char * s)

/*
 *
 * returns:
 *	The last character in the string or
 *	'\0' on a NULL or empty string
 *
 ****************************************/
{
  int result = '\0';
  if (s)
	{
	  int length;
	  length = strlen(s);
	  if (length > 0)
		result = s[length-1];
	}
  return result;
}



/****************************************
 *
 */

static const char *
xfe_netcaster_path(void)

/*
 * description:
 *	Determine a path to tab.htm, which is
 *	loaded into a browser window to start
 *	Netcaster by checking
 *	the following locations in order:
 *		$HOME/.netscape/netcast/tab.htm
 *		$MOZILLA_HOME/netcast/tab.htm
 *		Version Registry via VR_GetPath()
 *		fe_GetProgramDirectory()/netcast/tab.htm
 *
 *  The reason for precedence is as follows:
 *		$HOME first. Preferences set in a user's
 *		$HOME override everything else.
 *		This allows users some chance of running in
 *		the case where the system admin won't install
 *		Netcaster in globally accessible location.
 *		This can also be useful for debugging purposes.
 *
 *		$MOZILLA_HOME is now recommended as the standard
 *		for finding Communicator and related components.
 *		It comes next.
 *
 *		The registry should get set correctly during ASD install.
 *		However the registry's role in tracking versions has
 *		been emphasized over its role in tracking location.
 *		(although you can't have the former without the latter).
 *		Rumor also has it that some people are in the habit
 *		of deleting the registry to fix problems, so it
 *		may not always be available.
 *
 *		Last resort is to do our best effort at determining
 *		where Communicator was invoked by getting the program directory
 *		by looking at the invocation path.
 *
 * returns:
 *	On success returns a path to tab.htm
 *	On failure returns NULL
 *
 ****************************************/
{
  const char * result = 0;
  char * home;
  REGERR code;

  if (!private_xfe_netcaster_path)
	{
	  private_xfe_netcaster_path = (char*)XP_ALLOC(MAXPATHLEN);
	  if (private_xfe_netcaster_path)
		private_xfe_netcaster_path[0] = '\0';
	  else
		return result;
	}

  if (private_xfe_netcaster_path[0])
	{
	  result = private_xfe_netcaster_path;
#ifdef DEBUG_rodt
		  printf("DEBUG_rodt: Netcaster path a %s\n",result);
#endif
	  return result;
	}


  //
  // CHECK $HOME/.netscape
  //
  home = getenv("HOME");
  if (home)
	{
	  XP_STRCPY(private_xfe_netcaster_path, home);
	  if (xfe_last_character(private_xfe_netcaster_path) != '/')
		XP_STRCAT(private_xfe_netcaster_path,"/");
	  XP_STRCAT(private_xfe_netcaster_path,".netscape/");
	  XP_STRCAT(private_xfe_netcaster_path,netcasterTabHtmlPath);
	  if (xfe_path_exists(private_xfe_netcaster_path))
		{
		  result = private_xfe_netcaster_path;
#ifdef DEBUG_rodt
		  printf("DEBUG_rodt: Netcaster path b %s\n",result);
#endif
		  return result;
		}
	}


  //
  // CHECK $MOZILLA_HOME
  //
  home = getenv("MOZILLA_HOME");
  if (home)
	{
	  XP_STRCPY(private_xfe_netcaster_path, home);
	  if (xfe_last_character(private_xfe_netcaster_path) != '/')
		XP_STRCAT(private_xfe_netcaster_path,"/");
	  XP_STRCAT(private_xfe_netcaster_path,netcasterTabHtmlPath);
	  if (xfe_path_exists(private_xfe_netcaster_path))
		{
		  result = private_xfe_netcaster_path;
#ifdef DEBUG_rodt
		  printf("DEBUG_rodt: Netcaster path c %s\n",result);
#endif
		  return result;
		}
	}

  //
  // CHECK THE REGISTRY
  //
  // Could also call VR_InRegistry("Netcaster") but it would be redundant
  // Note that VR_ValidateComponent also calls VR_GetPath() but it doesn't
  // return any path information
  //
  code = VR_GetPath(netcasterTabHtmlRegistryNode, sizeof(private_xfe_netcaster_path)-1, private_xfe_netcaster_path);
  if (code == REGERR_OK)
	{
	  code = VR_ValidateComponent(netcasterTabHtmlRegistryNode);
	  if (code == REGERR_OK
		  && xfe_path_exists(private_xfe_netcaster_path))  // extra check
		{
		  result = private_xfe_netcaster_path;
#ifdef DEBUG_rodt
		  printf("DEBUG_rodt: Netcaster path d %s\n",result);
#endif
		  return result;
		}
	}
  private_xfe_netcaster_path[0] = '\0';

  //
  // CHECK THE PROGRAM DIRECTORY
  //
  fe_GetProgramDirectory(private_xfe_netcaster_path, MAXPATHLEN - 1);
  if (private_xfe_netcaster_path[0])
	{
	  if (xfe_last_character(private_xfe_netcaster_path) != '/')
		XP_STRCAT(private_xfe_netcaster_path,"/");
	  XP_STRCAT(private_xfe_netcaster_path,netcasterTabHtmlPath);
	  if (xfe_path_exists(private_xfe_netcaster_path))
		{
		  result = private_xfe_netcaster_path;
#ifdef DEBUG_rodt
		  printf("DEBUG_rodt: Netcaster path e %s\n",result);
#endif
		  return result;
		}
	}

  private_xfe_netcaster_path[0] = '\0';
#ifdef DEBUG_rodt
  printf("DEBUG_rodt: Netcaster path not found\n");
#endif
  return result;
}



/****************************************
 *
 */

static const char*
xfe_netcaster_url(void)

/*
 * returns:
 *	On success returns a file URL pointing to tab.html
 *	On failure returns NULL
 *
 ****************************************/
{
  const char * result = NULL;

  if (!private_xfe_netcaster_url)
	{
	  private_xfe_netcaster_url = (char*)XP_ALLOC(MAXPATHLEN);
	  if (private_xfe_netcaster_url)
		private_xfe_netcaster_url[0] = '\0';
	  else
		return result;
	}

  if (private_xfe_netcaster_url[0])
	{
	  result = private_xfe_netcaster_url;
	}
  else
    {
	  const char * path;
	  path = xfe_netcaster_path();

	  if (path)
		{
		  XP_SPRINTF(private_xfe_netcaster_url,"file:%s",path);
		  result = private_xfe_netcaster_url;
		}
	  else
		private_xfe_netcaster_url[0] = '\0';
	}

  return result;
}



/****************************************
 */

void
fe_showNetcaster(Widget toplevel)

/*
 * description:
 *	This function shows the Netcaster window, starting
 *	Netcaster if necessary.  If Netcaster is not installed,
 *  this function does nothing.
 *
 ****************************************/
{
  MWContext * netcasterContext;

  if (!fe_IsNetcasterInstalled())
    return;

  if(!(netcasterContext=FE_IsNetcasterRunning()))
	{ 

	  Chrome      netcasterChrome;
	  URL_Struct* URL_s;
	  const char* netcasterURL = xfe_netcaster_url();

      if (netcasterURL)
		{

		  if (!LM_GetMochaEnabled() || !LJ_GetJavaEnabled())
			{
			  fe_Alert_2(toplevel, XP_GetString(XP_ALERT_NETCASTER_NO_JS));
			  return;
			}

		  memset(&netcasterChrome, 0, sizeof(Chrome));

		  netcasterChrome.w_hint             = 21;
		  netcasterChrome.h_hint             = 59;
		  netcasterChrome.l_hint             = -1000;
		  netcasterChrome.t_hint             = -1000;
		  netcasterChrome.topmost            = TRUE;
		  netcasterChrome.z_lock             = TRUE;
		  netcasterChrome.location_is_chrome = TRUE;
		  netcasterChrome.disable_commands   = TRUE;
		  netcasterChrome.hide_title_bar     = TRUE;
		  netcasterChrome.restricted_target  = TRUE;
      
		  URL_s = NET_CreateURLStruct(netcasterURL, NET_DONT_RELOAD);
      
		  netcasterContext = xfe2_MakeNewWindow(toplevel,NULL,
												URL_s,
												netcasterWindowName,
												MWContextBrowser,
												False,
												&netcasterChrome);
		}
	  else
		{
		  fe_Alert_2(toplevel, XP_GetString(XP_ALERT_CANT_RUN_NETCASTER));
		} 
	}
  else
	{
	  FE_RaiseWindow(netcasterContext);
	}
}


/****************************************
 *
 */

XP_Bool
fe_IsNetcasterInstalled(void)

/*
 * returns:
 *	TRUE or FALSE
 ****************************************/
{
  XP_Bool result = FALSE;

  if (xfe_netcaster_path())
	result = TRUE;

  return result;
}
