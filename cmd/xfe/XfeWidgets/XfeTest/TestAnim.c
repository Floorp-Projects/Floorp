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
/*-----------------------------------------*/
/*																		*/
/* Name:		<XfeTest/TestAnim.c>									*/
/* Description:	Xfe widget animation tests stuff.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Main Animation 20x20													*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <animations/main/20x20/MainSmall00.xpm>
#include <animations/main/20x20/MainSmall01.xpm>
#include <animations/main/20x20/MainSmall02.xpm>
#include <animations/main/20x20/MainSmall03.xpm>
#include <animations/main/20x20/MainSmall04.xpm>
#include <animations/main/20x20/MainSmall05.xpm>
#include <animations/main/20x20/MainSmall06.xpm>
#include <animations/main/20x20/MainSmall07.xpm>
#include <animations/main/20x20/MainSmall08.xpm>
#include <animations/main/20x20/MainSmall09.xpm>
#include <animations/main/20x20/MainSmall10.xpm>
#include <animations/main/20x20/MainSmall11.xpm>
#include <animations/main/20x20/MainSmall12.xpm>
#include <animations/main/20x20/MainSmall13.xpm>
#include <animations/main/20x20/MainSmall14.xpm>
#include <animations/main/20x20/MainSmall15.xpm>
#include <animations/main/20x20/MainSmall16.xpm>
#include <animations/main/20x20/MainSmall17.xpm>
#include <animations/main/20x20/MainSmall18.xpm>
#include <animations/main/20x20/MainSmall19.xpm>
#include <animations/main/20x20/MainSmall20.xpm>
#include <animations/main/20x20/MainSmall21.xpm>
#include <animations/main/20x20/MainSmall22.xpm>
#include <animations/main/20x20/MainSmall23.xpm>
#include <animations/main/20x20/MainSmall24.xpm>
#include <animations/main/20x20/MainSmall25.xpm>
#include <animations/main/20x20/MainSmall26.xpm>
#include <animations/main/20x20/MainSmall27.xpm>
#include <animations/main/20x20/MainSmall28.xpm>
#include <animations/main/20x20/MainSmall29.xpm>
#include <animations/main/20x20/MainSmall30.xpm>
#include <animations/main/20x20/MainSmall31.xpm>
#include <animations/main/20x20/MainSmall32.xpm>
#include <animations/main/20x20/MainSmall33.xpm>
#include <animations/main/20x20/MainSmall34.xpm>

static char ** anim_main_20x20[] =
{
	anim_MainSmall00_xpm,
	anim_MainSmall01_xpm,
	anim_MainSmall02_xpm,
	anim_MainSmall03_xpm,
	anim_MainSmall04_xpm,
	anim_MainSmall05_xpm,
	anim_MainSmall06_xpm,
	anim_MainSmall07_xpm,
	anim_MainSmall08_xpm,
	anim_MainSmall09_xpm,
	anim_MainSmall10_xpm,
	anim_MainSmall11_xpm,
	anim_MainSmall12_xpm,
	anim_MainSmall13_xpm,
	anim_MainSmall14_xpm,
	anim_MainSmall15_xpm,
	anim_MainSmall16_xpm,
	anim_MainSmall17_xpm,
	anim_MainSmall18_xpm,
	anim_MainSmall19_xpm,
	anim_MainSmall20_xpm,
	anim_MainSmall21_xpm,
	anim_MainSmall22_xpm,
	anim_MainSmall23_xpm,
	anim_MainSmall24_xpm,
	anim_MainSmall25_xpm,
	anim_MainSmall26_xpm,
	anim_MainSmall27_xpm,
	anim_MainSmall28_xpm,
	anim_MainSmall29_xpm,
	anim_MainSmall30_xpm,
	anim_MainSmall31_xpm,
	anim_MainSmall32_xpm,
	anim_MainSmall33_xpm,
	anim_MainSmall34_xpm,
	NULL
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Main Animation 40x40													*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <animations/main/40x40/MainLarge00.xpm>
#include <animations/main/40x40/MainLarge01.xpm>
#include <animations/main/40x40/MainLarge02.xpm>
#include <animations/main/40x40/MainLarge03.xpm>
#include <animations/main/40x40/MainLarge04.xpm>
#include <animations/main/40x40/MainLarge05.xpm>
#include <animations/main/40x40/MainLarge06.xpm>
#include <animations/main/40x40/MainLarge07.xpm>
#include <animations/main/40x40/MainLarge08.xpm>
#include <animations/main/40x40/MainLarge09.xpm>
#include <animations/main/40x40/MainLarge10.xpm>
#include <animations/main/40x40/MainLarge11.xpm>
#include <animations/main/40x40/MainLarge12.xpm>
#include <animations/main/40x40/MainLarge13.xpm>
#include <animations/main/40x40/MainLarge14.xpm>
#include <animations/main/40x40/MainLarge15.xpm>
#include <animations/main/40x40/MainLarge16.xpm>
#include <animations/main/40x40/MainLarge17.xpm>
#include <animations/main/40x40/MainLarge18.xpm>
#include <animations/main/40x40/MainLarge19.xpm>
#include <animations/main/40x40/MainLarge20.xpm>
#include <animations/main/40x40/MainLarge21.xpm>
#include <animations/main/40x40/MainLarge22.xpm>
#include <animations/main/40x40/MainLarge23.xpm>
#include <animations/main/40x40/MainLarge24.xpm>
#include <animations/main/40x40/MainLarge25.xpm>
#include <animations/main/40x40/MainLarge26.xpm>
#include <animations/main/40x40/MainLarge27.xpm>
#include <animations/main/40x40/MainLarge28.xpm>
#include <animations/main/40x40/MainLarge29.xpm>
#include <animations/main/40x40/MainLarge30.xpm>
#include <animations/main/40x40/MainLarge31.xpm>
#include <animations/main/40x40/MainLarge32.xpm>
#include <animations/main/40x40/MainLarge33.xpm>
#include <animations/main/40x40/MainLarge34.xpm>

static char ** anim_main_40x40[] =
{
	anim_MainLarge00_xpm,
	anim_MainLarge01_xpm,
	anim_MainLarge02_xpm,
	anim_MainLarge03_xpm,
	anim_MainLarge04_xpm,
	anim_MainLarge05_xpm,
	anim_MainLarge06_xpm,
	anim_MainLarge07_xpm,
	anim_MainLarge08_xpm,
	anim_MainLarge09_xpm,
	anim_MainLarge10_xpm,
	anim_MainLarge11_xpm,
	anim_MainLarge12_xpm,
	anim_MainLarge13_xpm,
	anim_MainLarge14_xpm,
	anim_MainLarge15_xpm,
	anim_MainLarge16_xpm,
	anim_MainLarge17_xpm,
	anim_MainLarge18_xpm,
	anim_MainLarge19_xpm,
	anim_MainLarge20_xpm,
	anim_MainLarge21_xpm,
	anim_MainLarge22_xpm,
	anim_MainLarge23_xpm,
	anim_MainLarge24_xpm,
	anim_MainLarge25_xpm,
	anim_MainLarge26_xpm,
	anim_MainLarge27_xpm,
	anim_MainLarge28_xpm,
	anim_MainLarge29_xpm,
	anim_MainLarge30_xpm,
	anim_MainLarge31_xpm,
	anim_MainLarge32_xpm,
	anim_MainLarge33_xpm,
	anim_MainLarge34_xpm,
	NULL
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Transparent Animation 20x20											*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <animations/transparent/20x20/TransparentSmall00.xpm>
#include <animations/transparent/20x20/TransparentSmall01.xpm>
#include <animations/transparent/20x20/TransparentSmall02.xpm>
#include <animations/transparent/20x20/TransparentSmall03.xpm>
#include <animations/transparent/20x20/TransparentSmall04.xpm>
#include <animations/transparent/20x20/TransparentSmall05.xpm>
#include <animations/transparent/20x20/TransparentSmall06.xpm>
#include <animations/transparent/20x20/TransparentSmall07.xpm>
#include <animations/transparent/20x20/TransparentSmall08.xpm>
#include <animations/transparent/20x20/TransparentSmall09.xpm>
#include <animations/transparent/20x20/TransparentSmall10.xpm>
#include <animations/transparent/20x20/TransparentSmall11.xpm>
#include <animations/transparent/20x20/TransparentSmall12.xpm>
#include <animations/transparent/20x20/TransparentSmall13.xpm>
#include <animations/transparent/20x20/TransparentSmall14.xpm>
#include <animations/transparent/20x20/TransparentSmall15.xpm>
#include <animations/transparent/20x20/TransparentSmall16.xpm>
#include <animations/transparent/20x20/TransparentSmall17.xpm>
#include <animations/transparent/20x20/TransparentSmall18.xpm>
#include <animations/transparent/20x20/TransparentSmall19.xpm>
#include <animations/transparent/20x20/TransparentSmall20.xpm>
#include <animations/transparent/20x20/TransparentSmall21.xpm>
#include <animations/transparent/20x20/TransparentSmall22.xpm>
#include <animations/transparent/20x20/TransparentSmall23.xpm>
#include <animations/transparent/20x20/TransparentSmall24.xpm>
#include <animations/transparent/20x20/TransparentSmall25.xpm>
#include <animations/transparent/20x20/TransparentSmall26.xpm>
#include <animations/transparent/20x20/TransparentSmall27.xpm>
#include <animations/transparent/20x20/TransparentSmall28.xpm>
#include <animations/transparent/20x20/TransparentSmall29.xpm>
#include <animations/transparent/20x20/TransparentSmall30.xpm>
#include <animations/transparent/20x20/TransparentSmall31.xpm>
#include <animations/transparent/20x20/TransparentSmall32.xpm>
#include <animations/transparent/20x20/TransparentSmall33.xpm>
#include <animations/transparent/20x20/TransparentSmall34.xpm>

static char ** anim_transparent_20x20[] =
{
	anim_TransparentSmall00_xpm,
	anim_TransparentSmall01_xpm,
	anim_TransparentSmall02_xpm,
	anim_TransparentSmall03_xpm,
	anim_TransparentSmall04_xpm,
	anim_TransparentSmall05_xpm,
	anim_TransparentSmall06_xpm,
	anim_TransparentSmall07_xpm,
	anim_TransparentSmall08_xpm,
	anim_TransparentSmall09_xpm,
	anim_TransparentSmall10_xpm,
	anim_TransparentSmall11_xpm,
	anim_TransparentSmall12_xpm,
	anim_TransparentSmall13_xpm,
	anim_TransparentSmall14_xpm,
	anim_TransparentSmall15_xpm,
	anim_TransparentSmall16_xpm,
	anim_TransparentSmall17_xpm,
	anim_TransparentSmall18_xpm,
	anim_TransparentSmall19_xpm,
	anim_TransparentSmall20_xpm,
	anim_TransparentSmall21_xpm,
	anim_TransparentSmall22_xpm,
	anim_TransparentSmall23_xpm,
	anim_TransparentSmall24_xpm,
	anim_TransparentSmall25_xpm,
	anim_TransparentSmall26_xpm,
	anim_TransparentSmall27_xpm,
	anim_TransparentSmall28_xpm,
	anim_TransparentSmall29_xpm,
	anim_TransparentSmall30_xpm,
	anim_TransparentSmall31_xpm,
	anim_TransparentSmall32_xpm,
	anim_TransparentSmall33_xpm,
	anim_TransparentSmall34_xpm,
	NULL
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Transparent Animation 40x40											*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <animations/transparent/40x40/TransparentLarge00.xpm>
#include <animations/transparent/40x40/TransparentLarge01.xpm>
#include <animations/transparent/40x40/TransparentLarge02.xpm>
#include <animations/transparent/40x40/TransparentLarge03.xpm>
#include <animations/transparent/40x40/TransparentLarge04.xpm>
#include <animations/transparent/40x40/TransparentLarge05.xpm>
#include <animations/transparent/40x40/TransparentLarge06.xpm>
#include <animations/transparent/40x40/TransparentLarge07.xpm>
#include <animations/transparent/40x40/TransparentLarge08.xpm>
#include <animations/transparent/40x40/TransparentLarge09.xpm>
#include <animations/transparent/40x40/TransparentLarge10.xpm>
#include <animations/transparent/40x40/TransparentLarge11.xpm>
#include <animations/transparent/40x40/TransparentLarge12.xpm>
#include <animations/transparent/40x40/TransparentLarge13.xpm>
#include <animations/transparent/40x40/TransparentLarge14.xpm>
#include <animations/transparent/40x40/TransparentLarge15.xpm>
#include <animations/transparent/40x40/TransparentLarge16.xpm>
#include <animations/transparent/40x40/TransparentLarge17.xpm>
#include <animations/transparent/40x40/TransparentLarge18.xpm>
#include <animations/transparent/40x40/TransparentLarge19.xpm>
#include <animations/transparent/40x40/TransparentLarge20.xpm>
#include <animations/transparent/40x40/TransparentLarge21.xpm>
#include <animations/transparent/40x40/TransparentLarge22.xpm>
#include <animations/transparent/40x40/TransparentLarge23.xpm>
#include <animations/transparent/40x40/TransparentLarge24.xpm>
#include <animations/transparent/40x40/TransparentLarge25.xpm>
#include <animations/transparent/40x40/TransparentLarge26.xpm>
#include <animations/transparent/40x40/TransparentLarge27.xpm>
#include <animations/transparent/40x40/TransparentLarge28.xpm>
#include <animations/transparent/40x40/TransparentLarge29.xpm>
#include <animations/transparent/40x40/TransparentLarge30.xpm>
#include <animations/transparent/40x40/TransparentLarge31.xpm>
#include <animations/transparent/40x40/TransparentLarge32.xpm>
#include <animations/transparent/40x40/TransparentLarge33.xpm>
#include <animations/transparent/40x40/TransparentLarge34.xpm>

static char ** anim_transparent_40x40[] =
{
	anim_TransparentLarge00_xpm,
	anim_TransparentLarge01_xpm,
	anim_TransparentLarge02_xpm,
	anim_TransparentLarge03_xpm,
	anim_TransparentLarge04_xpm,
	anim_TransparentLarge05_xpm,
	anim_TransparentLarge06_xpm,
	anim_TransparentLarge07_xpm,
	anim_TransparentLarge08_xpm,
	anim_TransparentLarge09_xpm,
	anim_TransparentLarge10_xpm,
	anim_TransparentLarge11_xpm,
	anim_TransparentLarge12_xpm,
	anim_TransparentLarge13_xpm,
	anim_TransparentLarge14_xpm,
	anim_TransparentLarge15_xpm,
	anim_TransparentLarge16_xpm,
	anim_TransparentLarge17_xpm,
	anim_TransparentLarge18_xpm,
	anim_TransparentLarge19_xpm,
	anim_TransparentLarge20_xpm,
	anim_TransparentLarge21_xpm,
	anim_TransparentLarge22_xpm,
	anim_TransparentLarge23_xpm,
	anim_TransparentLarge24_xpm,
	anim_TransparentLarge25_xpm,
	anim_TransparentLarge26_xpm,
	anim_TransparentLarge27_xpm,
	anim_TransparentLarge28_xpm,
	anim_TransparentLarge29_xpm,
	anim_TransparentLarge30_xpm,
	anim_TransparentLarge31_xpm,
	anim_TransparentLarge32_xpm,
	anim_TransparentLarge33_xpm,
	anim_TransparentLarge34_xpm,
	NULL
};


/*----------------------------------------------------------------------*/
void
XfeAnimationCreate(Widget			w,
				   char ***			animation_data,
				   Pixmap **		pixmaps_out,
				   Cardinal *		num_pixmaps_out)
{
	Pixmap *		pixmaps;
	Cardinal		num_pixmaps;
	Cardinal		i;

	assert( XfeIsAlive(w) );
	assert( animation_data != NULL );
	assert( pixmaps_out != NULL );
	assert( *pixmaps_out == NULL );
	assert( num_pixmaps_out != NULL );

	/* First count the number of entries in the pixmap data */
	i = 0;

	while(animation_data[i] != NULL)
	{
		i++;
	}

	num_pixmaps = i;

	/* Allocate space for the pixmaps */
	pixmaps = (Pixmap *) XtMalloc(sizeof(Pixmap) * num_pixmaps);
	
	for (i = 0; i < num_pixmaps; i++)
	{
		pixmaps[i] = XfeGetPixmapFromData(w,animation_data[i]);
	}

	*pixmaps_out		= pixmaps;
	*num_pixmaps_out	= num_pixmaps;
}
/*----------------------------------------------------------------------*/
void
XfeGetMain20x20Animation(Widget		w,
						 Pixmap **	pixmaps_out,
						 Cardinal *	num_pixmaps_out)
{
	XfeAnimationCreate(w,anim_main_20x20,pixmaps_out,num_pixmaps_out);
}
/*----------------------------------------------------------------------*/
void
XfeGetMain40x40Animation(Widget		w,
						 Pixmap **	pixmaps_out,
						 Cardinal *	num_pixmaps_out)
{
	XfeAnimationCreate(w,anim_main_40x40,pixmaps_out,num_pixmaps_out);
}
/*----------------------------------------------------------------------*/
void
XfeGetTransparent20x20Animation(Widget		w,
								Pixmap **	pixmaps_out,
								Cardinal *	num_pixmaps_out)
{
	XfeAnimationCreate(w,anim_transparent_20x20,pixmaps_out,num_pixmaps_out);
}
/*----------------------------------------------------------------------*/
void
XfeGetTransparent40x40Animation(Widget		w,
								Pixmap **	pixmaps_out,
								Cardinal *	num_pixmaps_out)
{
	XfeAnimationCreate(w,anim_transparent_40x40,pixmaps_out,num_pixmaps_out);
}
/*----------------------------------------------------------------------*/
