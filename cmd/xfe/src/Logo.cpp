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
/*---------------------------------------*/
#include "prefapi.h"
/*																		*/
/* Name:		Logo.cpp												*/
/* Description:	XFE_Logo component source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include "Logo.h"
#include "xpassert.h"
#include "MozillaApp.h"
#include "icondata.h"
#include "e_kit.h"

#include <Xfe/XfeAll.h>


/* static */ Pixmap **		
XFE_Logo::_small_animation_pixmaps = NULL;

/* static */ fe_icon_data **
XFE_Logo::_small_animation_data = NULL;

/* static */ Pixmap **		
XFE_Logo::_large_animation_pixmaps = NULL;

/* static */ fe_icon_data **
XFE_Logo::_large_animation_data = NULL;

/* static */ Cardinal		 	
XFE_Logo::_num_animations = 0;

/* static */ Cardinal *		
XFE_Logo::_animation_frame_counts = NULL;

#define DEFAULT_ANIMATION		XFE_ANIMATION_MAIN
#define DEFAULT_SIZE			XFE_ANIMATION_LARGE

// baggage
extern "C" 
{
	void fe_NetscapeCallback(Widget, XtPointer, XtPointer);
};

//////////////////////////////////////////////////////////////////////////
XFE_Logo::XFE_Logo(XFE_Frame *		frame,
				   Widget			parent,
				   const char *		name,
				   int				/* interval */) :
	XFE_Component(frame),
	_frame(frame),
#ifdef NETSCAPE_PRIV
	_mozillaEasterEgg(False),
#endif /* NETSCAPE_PRIV */
	_type(DEFAULT_ANIMATION),
	_size(DEFAULT_SIZE)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	// Create the base widget - a tool item
	m_widget = XtVaCreateWidget(name,
								xfeLogoWidgetClass,
								parent,

								XmNtraversalOn,			True,
								XmNhighlightThickness,	0,

								NULL);

	// Add tooltip to logo
	fe_AddTipStringCallback(m_widget, XFE_Logo::tipStringCallback, NULL);

	// Add the netscape callback
	XtAddCallback(m_widget,
				  XmNactivateCallback,
				  fe_NetscapeCallback, 
				  (XtPointer) _frame->getContext());

	XtAddCallback(m_widget,
				  XmNdisarmCallback,
				  &XFE_Logo::logoCallback,
				  (XtPointer) this);
				  
	XtAddCallback(m_widget,
				  XmNactivateCallback,
				  &XFE_Logo::logoCallback,
				  (XtPointer) this);

    if ( ekit_CustomAnimation() ) {
        _type = XFE_ANIMATION_CUSTOM;
    }

	// Initizlize the animations if they are not valid
	if (!XFE_Logo::animationsAreValid())
	{
		XFE_Logo::initAnimationData();
	}

	updatePixmaps();

	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////
XFE_Logo::~XFE_Logo()
{
}
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
//
// Accessors
//
//////////////////////////////////////////////////////////////////////////
Cardinal
XFE_Logo::getCurrentFrame()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Cardinal index;
	
	XtVaGetValues(m_widget,XmNcurrentPixmapIndex,&index,NULL);

	return index;
}
//////////////////////////////////////////////////////////////////////////
EAnimationSize
XFE_Logo::getSize()
{
	return _size;
}
//////////////////////////////////////////////////////////////////////////
EAnimationType
XFE_Logo::getType()
{
	return _type;
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Logo::isRunning()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	Boolean is_running;

	XtVaGetValues(m_widget,XmNanimationRunning,&is_running,NULL);

	return is_running;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Mutators
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::setCurrentFrame(Cardinal index)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XtVaSetValues(m_widget,XmNcurrentPixmapIndex,index,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::setInterval(Cardinal interval)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( interval > 0 );

	XtVaSetValues(m_widget,XmNanimationInterval,interval,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::setSize(EAnimationSize size)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XP_ASSERT( size >= XFE_ANIMATION_LARGE );
	XP_ASSERT( size <= XFE_ANIMATION_SMALL );

	if (_size == size)
	{
		return;
	}

	_size = size;

	updatePixmaps();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::setType(EAnimationType type)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XP_ASSERT( type >= 0 );
	XP_ASSERT( type < XFE_ANIMATION_MAX );

#ifdef NETSCAPE_PRIV
    // Mozilla rules
    if (_mozillaEasterEgg)
    {
		_type = XFE_ANIMATION_MOZILLA;

		updatePixmaps();

        return;
    }
#endif /* NETSCAPE_PRIV */

	if (_type == type || _type == XFE_ANIMATION_CUSTOM)
	{
		return;
	}

	_type = type;

	updatePixmaps();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Control methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::reset()
{
 	XP_ASSERT( XfeIsAlive(m_widget) );

 	XfeLogoAnimationReset(m_widget);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::start()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeLogoAnimationStart(m_widget);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::stop()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeLogoAnimationStop(m_widget);

#ifdef NETSCAPE_PRIV
	// For compass or mozilla, we want the animation to continue where it
	// stopped.  Tell the logo widget not too reset the pixmap when its idle.
	if ((_type == XFE_ANIMATION_COMPASS) ||
		(_type == XFE_ANIMATION_MOZILLA))
	{
		XtVaSetValues(m_widget,XmNresetWhenIdle,False,NULL);
	}
	else
#endif /* NETSCAPE_PRIV */
	{
		XtVaSetValues(m_widget,XmNresetWhenIdle,True,NULL);
		setCurrentFrame(0);
	}
}
//////////////////////////////////////////////////////////////////////////

#ifdef NETSCAPE_PRIV

// Only Jamie gets the compass, everyone else gets MOZILLA.
#define JWZ 9

static String _ee_xheads[] =
{
    "\244\256\256\244\261\244",				// akkana
    "\245\265\254\244\261\262",				// briano
    "\245\266\267\250\257\257",				// bstell
    "\246\262\261\271\250\265\266\250",		// converse
    "\247\255\272",							// djw
    "\247\262\265\244",						// dora
    "\247\263",								// dp
    "\251\265\244\261\246\254\266",			// francis
    "\256\254\261",							// kin
    "\255\272\275",							// jwz
    "\257\272\250\254",						// lwei
    "\260\246\244\251\250\250",				// mcafee
    "\265\244\247\253\244",					// radha
    "\265\244\260\254\265\262",				// ramiro
    "\265\253\250\266\266",					// rhess
    "\265\262\247\267",						// rodt
    "\266\257\244\260\260",					// slamm
    "\266\263\250\261\246\250",				// spence
    "\267\244\262",							// tao
    "\267\262\266\253\262\256",				// toshok
    "\275\255\244\261\245\244\274"			// zjanbay
};

static String _ee_home_urls[] =
{
	/*
	 * http://home.netscape.com/people/ 
	 */
	"\253\267\267\263\175\162\162\253\262\260\250"
	"\161\261\250\267\266\246\244\263\250"
	"\161\246\262\260\162\263\250\262\263\257\250\162",

	/*
	 * http://www.netscape.com/people/ 
	 */
	"\253\267\267\263\175\162\162\272\272\272"
	"\161\261\250\267\266\246\244\263\250"
	"\161\246\262\260\162\263\250\262\263\257\250\162",

	/* 
	 * http://people.netscape.com/ 
	 */
	"\253\267\267\263\175\162\162\263\250\262"
	"\263\257\250\161\261\250\267\266\246\244"
	"\263\250\161\246\262\260\162",
};

#define _EE_NUM_XHEADS ((int) XtNumber(_ee_xheads))

static String _ee_xheads_bufs[_EE_NUM_XHEADS];

static Cardinal _ee_xheads_lengths[_EE_NUM_XHEADS];

#define _EE_NUM_HOME_URLS ((int) XtNumber(_ee_home_urls))

static String _ee_home_urls_bufs[_EE_NUM_HOME_URLS];

static Cardinal _ee_home_urls_lengths[_EE_NUM_HOME_URLS];

#define _EE_MAGIC_NUMBER 67

static Boolean _ee_first_time = True;

static String _ee_about_mozilla =
"\244\245\262\270\267\175\260\262\275\254\257\257\244";

static String _ee_about_mozilla_buf;

//////////////////////////////////////////////////////////////////////////
static void _ee_un_obfuscate(char * s)
{
    if (!s) 
        return;

    char * p = s; 

    while(*p) 
    { 
        *p -= _EE_MAGIC_NUMBER; p++; 
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::easterEgg(URL_Struct * url)
{
    // Sanity check
    if (!url || !url->address || !*url->address)
    {
        return;
    }

	int i;

	// The "first time" we allocate non static memory for all the easter 
	// egg strings and un-obfuscate them
	if (_ee_first_time)
    {
        _ee_first_time = False;

		_ee_about_mozilla_buf = (String) XtNewString(_ee_about_mozilla);
        _ee_un_obfuscate(_ee_about_mozilla_buf);

        for(i = 0; i < _EE_NUM_XHEADS; i++)
        {
			_ee_xheads_bufs[i] = (String) XtNewString(_ee_xheads[i]);

            _ee_un_obfuscate(_ee_xheads_bufs[i]);

			_ee_xheads_lengths[i] = XP_STRLEN(_ee_xheads_bufs[i]);
        }

        for(i = 0; i < _EE_NUM_HOME_URLS; i++)
        {
			_ee_home_urls_bufs[i] = (String) XtNewString(_ee_home_urls[i]);

            _ee_un_obfuscate(_ee_home_urls_bufs[i]);

			_ee_home_urls_lengths[i] = XP_STRLEN(_ee_home_urls_bufs[i]);
        }
    }

	// Look for about:mozilla
	if (XP_STRCMP(url->address,_ee_about_mozilla_buf) == 0)
	{
		_mozillaEasterEgg = True;

		setType(XFE_ANIMATION_MOZILLA);

		return;
	}

	XP_Bool found_xheads_prefix = False;
	int		xheads_prefix_offset = 0;

	i = 0;

	// Look for one of the xheads home url prefixes
	while ((i < _EE_NUM_HOME_URLS) && !found_xheads_prefix)
	{
		if (XP_STRNCMP(url->address,
					   _ee_home_urls_bufs[i],
					   _ee_home_urls_lengths[i]) == 0)
		{
			found_xheads_prefix = True;
			xheads_prefix_offset = _ee_home_urls_lengths[i];
		}

		i++;
	}

	// Look for one of the xheads
	if (found_xheads_prefix)
	{
		char * possible_xhead = url->address + xheads_prefix_offset;

		if (possible_xhead && *possible_xhead)
		{
			// Look for jwz
			if (XP_STRNCMP(possible_xhead,
						   _ee_xheads_bufs[JWZ],
						   _ee_xheads_lengths[JWZ]) == 0) {
				setType(XFE_ANIMATION_COMPASS);
				
				XtVaSetValues(m_widget,XmNresetWhenIdle,False,NULL);
				
				return;
			}
			
			// Look for xheads
			for(i = 0; i < _EE_NUM_XHEADS; i++)
			{
				if (XP_STRNCMP(possible_xhead,
							   _ee_xheads_bufs[i],
							   _ee_xheads_lengths[i]) == 0)
				{
					setType(XFE_ANIMATION_MOZILLA);

					XtVaSetValues(m_widget,XmNresetWhenIdle,False,NULL);

					return;
				}
			}
		}
	}

	// Default to the 4.x animation
	setType(XFE_ANIMATION_XNETSCAPE4);

	XtVaSetValues(m_widget,XmNresetWhenIdle,True,NULL);
}

#endif /* NETSCAPE_PRIV */
//////////////////////////////////////////////////////////////////////////

/* static */ Pixmap *
XFE_Logo::createAnimation(XFE_Frame *		frame,
						  Widget			logo,
						  fe_icon_data *	animation_data,
						  Cardinal			num_frames)
{
	XP_ASSERT( frame != NULL );
	XP_ASSERT( animation_data != NULL );
	XP_ASSERT( num_frames > 0 );

	Cardinal	i;
	Pixmap *	pixmaps = NULL;

	pixmaps = new Pixmap[num_frames];

	for(i = 0; i < num_frames; i++)
	{
		pixmaps[i] = XFE_Logo::createPixmap(frame,logo,&animation_data[i]);
	}

	return pixmaps;
}
//////////////////////////////////////////////////////////////////////////
/* static */ Pixmap
XFE_Logo::createPixmap(XFE_Frame *		frame,
					   Widget			logo,
					   fe_icon_data *	icon_data)
{
	XP_ASSERT( frame != NULL );
	XP_ASSERT( XfeIsAlive(logo) );
	XP_ASSERT( icon_data != NULL );

	fe_icon icon;

	icon.pixmap = 0;

	fe_NewMakeIcon(frame->getBaseWidget(),
				   XfeForeground(logo),
				   XfeBackground(logo),
				   &icon,
				   NULL,
				   icon_data->width,
				   icon_data->height,
				   icon_data->mono_bits,
				   icon_data->color_bits,
				   icon_data->mask_bits,
				   False);

	assert( XfePixmapGood(icon.pixmap) );

	return icon.pixmap;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_Logo::animationsAreValid()
{
	return (_small_animation_pixmaps &&
			_large_animation_pixmaps &&
			_large_animation_data &&
			_small_animation_data &&
			_small_animation_data);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Logo::initAnimationData()
{
	assert( !XFE_Logo::animationsAreValid() );

	// Allocate space for the pixmaps and data
	_num_animations = 4;
	
	_small_animation_pixmaps = new Pixmap * [_num_animations];
	_small_animation_data = new fe_icon_data * [_num_animations];

	_large_animation_pixmaps = new Pixmap * [_num_animations];
	_large_animation_data = new fe_icon_data * [_num_animations];

	_animation_frame_counts = new Cardinal [_num_animations];


	// Initialize all the pixmaps and data
	Cardinal i;

	for (i = 0; i < _num_animations; i++)
	{
		_small_animation_pixmaps[i] = NULL;
		_small_animation_data[i] = NULL;

		_large_animation_pixmaps[i] = NULL;
		_large_animation_data[i] = NULL;

		_animation_frame_counts[i] = 0;
	}

	// Initialize the animation counts
	_small_animation_data	[XFE_ANIMATION_MAIN] = anim_main_small;
	_large_animation_data	[XFE_ANIMATION_MAIN] = anim_main_large;
	_animation_frame_counts	[XFE_ANIMATION_MAIN] = fe_anim_frames[XFE_ANIMATION_MAIN*2];

#ifdef NETSCAPE_PRIV
	_small_animation_data	[XFE_ANIMATION_COMPASS]  = anim_compass_small;
	_large_animation_data	[XFE_ANIMATION_COMPASS]  = anim_compass_large;
	_animation_frame_counts	[XFE_ANIMATION_COMPASS]  = fe_anim_frames[XFE_ANIMATION_COMPASS*2];

	_small_animation_data	[XFE_ANIMATION_MOZILLA]  = anim_mozilla_small;
	_large_animation_data	[XFE_ANIMATION_MOZILLA]  = anim_mozilla_large;
	_animation_frame_counts	[XFE_ANIMATION_MOZILLA]  = fe_anim_frames[XFE_ANIMATION_MOZILLA*2];
#endif /* NETSCAPE_PRIV */

    // Set at startup by ekit_LoadCustomAnimation()
	if ( anim_custom_small && anim_custom_large ) {
		_small_animation_data	[XFE_ANIMATION_CUSTOM]  = anim_custom_small;
		_large_animation_data	[XFE_ANIMATION_CUSTOM]  = anim_custom_large;
		_animation_frame_counts	[XFE_ANIMATION_CUSTOM]  = fe_anim_frames[XFE_ANIMATION_CUSTOM*2];
	} else {
		// This shouldn't happen, but just in case...
		_small_animation_data	[XFE_ANIMATION_CUSTOM]  = anim_main_small;
		_large_animation_data	[XFE_ANIMATION_CUSTOM]  = anim_main_large;
		_animation_frame_counts	[XFE_ANIMATION_CUSTOM]  = fe_anim_frames[XFE_ANIMATION_MAIN*2];
	}

}
//////////////////////////////////////////////////////////////////////////
void
XFE_Logo::updatePixmaps()
{
	Pixmap *		pixmaps = NULL;
	Cardinal		num_pixmaps = 0;

	switch(_size)
	{
	case XFE_ANIMATION_LARGE:

		if (!_large_animation_pixmaps[_type])
		{
			_large_animation_pixmaps[_type] =
				XFE_Logo::createAnimation(_frame,
										  m_widget,
										  _large_animation_data[_type],
										  _animation_frame_counts[_type]);
		}

		pixmaps = _large_animation_pixmaps[_type];

		num_pixmaps = _animation_frame_counts[_type];
		
		break;

	case XFE_ANIMATION_SMALL:

		if (!_small_animation_pixmaps[_type])
		{
			_small_animation_pixmaps[_type] =
				XFE_Logo::createAnimation(_frame,
										  m_widget,
										  _small_animation_data[_type],
										  _animation_frame_counts[_type]);
		}

		pixmaps = _small_animation_pixmaps[_type];

		num_pixmaps = _animation_frame_counts[_type];

		break;
	}

#if 1
	// Set the default 'rest' pixmap
	XtVaSetValues(m_widget,
   				  XmNpixmap,				pixmaps[0],
				  NULL);

	// Skip the first pixmap which is the default 'rest' pixmap
	pixmaps++;
	num_pixmaps--;
#endif

	// Set the animation pixmaps
	XtVaSetValues(m_widget,
   				  XmNanimationPixmaps,		pixmaps,
				  XmNnumAnimationPixmaps,	num_pixmaps,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////

/* static */ void
XFE_Logo::logoCallback(Widget /* w */,XtPointer clientData,XtPointer callData)
{
	XFE_Logo *					logo = (XFE_Logo *) clientData;
	XfeButtonCallbackStruct *	cbs = (XfeButtonCallbackStruct *) callData;

	XP_ASSERT( logo != NULL );
    if (!logo) return;

	switch(cbs->reason)
	{
	case XmCR_ARM:

		break;
		
	case XmCR_DISARM:
		
		break;
	}
}

// XFE_Logo::tipStringCallback
// Checks to see if a config pref has been set to overrule the tooltip
// string or documentation string.
/* static */ void
XFE_Logo::tipStringCallback(Widget, XtPointer closure, XtPointer call_data)
{
	XFE_TipStringCallbackStruct* cb_info = (XFE_TipStringCallbackStruct*) call_data;
	char* pref;
	char* value;

	if ( !cb_info ) return;

	pref = ( cb_info->reason == XFE_DOCSTRING ) ? "toolbar.logo.doc_string" :
		   ( cb_info->reason == XFE_TIPSTRING ) ? "toolbar.logo.tooltip" :
		   (char *)NULL;

	if ( pref && PREF_CopyConfigString(pref, &value) == PREF_OK ) {
		*(cb_info->string) = value;
	}
}

//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Logo::processTraversal(XmTraversalDirection direction)
{
	if (m_widget && XfeIsAlive(m_widget) && 
		XmProcessTraversal(m_widget,direction))
	{
		return True;
	}

	return False;
}
//////////////////////////////////////////////////////////////////////////
