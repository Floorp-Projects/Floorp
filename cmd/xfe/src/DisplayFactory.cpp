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
   DisplayFactory.cpp -- class definition for the factory class
   Created: Spence Murray <spence@netscape.com>, 17-Sep-96.
 */



#include "DisplayFactory.h"
#include "xpassert.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#define PRIVATE_COLORMAP_REALLY_MEANS_PRIVATE_COLORMAP

static XFE_DisplayFactory *factoryRunning = NULL;

// Internationalization stuff
#include "xpgetstr.h"
extern int XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR;


fe_colormap *
XFE_DisplayFactory::getSharedColormap()
{
  // make sure the visual stuff has been dealt with.
  getVisual();

  if (m_sharedCmap == NULL)
    {
      if (m_alwaysInstallCmap)
	{
	  Colormap cmap = XCreateColormap (m_display, RootWindowOfScreen (m_screen),
					   m_visual, AllocNone);
	  m_sharedCmap = fe_NewColormap(m_screen, m_visual, cmap, True);
	}
      else
	{
	  Colormap cmap = DefaultColormapOfScreen(m_screen);
	  
	  m_sharedCmap = fe_NewColormap(m_screen, DefaultVisualOfScreen(m_screen), cmap, False);
	}
    }

  return m_sharedCmap;
}

fe_colormap *
XFE_DisplayFactory::getPrivateColormap()
{
#ifdef PRIVATE_COLORMAP_REALLY_MEANS_PRIVATE_COLORMAP

  if (m_alwaysInstallCmap)
    {
      Colormap cmap;

      // make sure the visual stuff has been dealt with.
      getVisual();

      cmap = XCreateColormap (m_display, RootWindowOfScreen (m_screen),
			      m_visual, AllocNone);

      return fe_NewColormap(m_screen, m_visual, cmap, True);
    }
  else
#endif
    return getSharedColormap();
}

Visual *
XFE_DisplayFactory::getVisual()
{
  if (m_visual == NULL)
  {
    m_visual = fe_globalData.default_visual;

    if (m_visual == NULL)
      {
        Visual *v;
        String str = 0;
  
        /* "*visualID" is special for a number of reasons... */
        static XtResource getVisual_res = { "visualID", "VisualID",
				  XtRString, sizeof (String),
				  0, XtRString, "default" };
  
        XtGetSubresources (m_toplevel, &str, (char *)fe_progname, "TopLevelShell",
			   &getVisual_res, 1, 0, 0);
        v = fe_ParseVisual (m_screen, str);
  
        m_visual =
	  fe_globalData.default_visual = v;
      }
  
    /* Don't allow colormap flashing on a deep display */
    if (m_visual != DefaultVisualOfScreen (m_screen))
      m_alwaysInstallCmap = True;

    if (!fe_globalData.default_colormap)
      {
	Colormap cmap = DefaultColormapOfScreen (m_screen);
	fe_globalData.default_colormap =
          fe_NewColormap(m_screen,  DefaultVisualOfScreen(m_screen), cmap, False);
      }
  }

  return m_visual;
}

int
XFE_DisplayFactory::getVisualDepth()
{
  // make sure the visual has been initialized
  getVisual();

  if (m_visualDepth == 0)
    {
      XVisualInfo vi_in, *vi_out;
      int out_count;
      vi_in.visualid = XVisualIDFromVisual (m_visual);
      vi_out = XGetVisualInfo (m_display, VisualIDMask,
			       &vi_in, &out_count);
      if (! vi_out) abort ();

      m_visualDepth = vi_out [0].depth;

      XFree ((char *) vi_out);
    }


  return m_visualDepth;
}

int
XFE_DisplayFactory::getVisualPixmapDepth()
{
  int pixmap_depth;
  int i, pfvc = 0;
  XPixmapFormatValues *pfv;

  // make sure the visual stuff has been initialized
  getVisual();
  getVisualDepth();
  
  pixmap_depth = m_visualDepth;

  pfv = XListPixmapFormats(m_display, &pfvc);
  /* Return the first matching depth in the pixmap formats.
     If there are no matching pixmap formats (which shouldn't
     be able to happen) return the visual depth instead. */
  for (i = 0; i < pfvc; i++)
    if (pfv[i].depth == m_visualDepth)
      {
	pixmap_depth = pfv[i].bits_per_pixel;
	break;
      }

  if (pfv)
    XFree(pfv);

  return pixmap_depth;
}

XFE_DisplayFactory::XFE_DisplayFactory(Widget toplevel)
{
  m_sharedCmap    = NULL;
  m_visual = NULL;
  m_visualDepth   = 0;

  m_toplevel = toplevel;

  m_display = XtDisplay(m_toplevel);
  m_screen = XtScreen(m_toplevel);

  {
    String str = 0;
    static XtResource res = { "installColormap", XtCString, XtRString,
			      sizeof (String), 0, XtRString, "guess" };
    XtGetApplicationResources (m_toplevel, &str, &res, 1, 0, 0);
    if (!str || !*str || !XP_STRCASECMP (str, "guess"))
#if 0
      /* If the server is capable of installing multiple hardware colormaps
	 simultaniously, take one for ourself by default.  Otherwise, don't. */
      m_alwaysInstallCmap = (MaxCmapsOfScreen (m_screen) > 1);
#else
    {
      /* But everybody lies about this value. */
      char *vendor = XServerVendor (m_display);
      m_alwaysInstallCmap =
	!strcmp (vendor, "Silicon Graphics");
    }
#endif
    else if (!XP_STRCASECMP (str, "yes") || !XP_STRCASECMP (str, "true"))
      m_alwaysInstallCmap = True;
    else if (!XP_STRCASECMP (str, "no") || !XP_STRCASECMP (str, "false"))
      m_alwaysInstallCmap = False;
    else
      {

//    XP_GetString ( XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR )
		  
// 		  fprintf (stderr,
// 				   "%s: installColormap: %s must be yes, no, or guess.\n",
// 				   fe_progname, str);

 		  fprintf(stderr,
				  XP_GetString(XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR),
				  fe_progname,
				  str);
		  
		  m_alwaysInstallCmap = False;
      }
  }
}

XFE_DisplayFactory::~XFE_DisplayFactory()
{
D(	printf ("in XFE_DisplayFactory::~XFE_DisplayFactory()\n");)

D(	printf ("leaving XFE_DisplayFactory::~XFE_DisplayFactory()\n");)
}

XFE_DisplayFactory *
XFE_DisplayFactory::theFactory()
{
  XP_ASSERT(factoryRunning);

  return factoryRunning;
}

void
XFE_DisplayFactory::colormapGoingAway(fe_colormap *colormap)
{
  if (colormap == m_sharedCmap)
    m_sharedCmap = NULL;
  /* we don't free it here, as it's freed inside fe_DisposeColormap */
}

void
fe_startDisplayFactory(Widget toplevel)
{
  if (factoryRunning == NULL)
    factoryRunning = new XFE_DisplayFactory(toplevel);
}

void
fe_DisplayFactoryColormapGoingAway(fe_colormap *colormap)
{
  XFE_DisplayFactory::theFactory()->colormapGoingAway(colormap);
}

fe_colormap * 
fe_GetSharedColormap()
{
	return XFE_DisplayFactory::theFactory()->getSharedColormap();
}
