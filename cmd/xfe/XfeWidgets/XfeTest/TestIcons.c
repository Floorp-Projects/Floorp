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
/* Name:		<XfeTest/TestIcons.c>									*/
/* Description:	Xfe widget icon test funcs.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* TaskBar pixmaps														*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <taskbar/task_browser.xpm>
#include <taskbar/task_browser_armed.xpm>
#include <taskbar/task_browser_raised.xpm>
#include <taskbar/task_editor.xpm>
#include <taskbar/task_editor_armed.xpm>
#include <taskbar/task_editor_raised.xpm>
#include <taskbar/task_mail.xpm>
#include <taskbar/task_mail_armed.xpm>
#include <taskbar/task_mail_raised.xpm>
#include <taskbar/task_news.xpm>
#include <taskbar/task_news_armed.xpm>
#include <taskbar/task_news_raised.xpm>
#include <taskbar/task_small_browser.xpm>
#include <taskbar/task_small_browser_armed.xpm>
#include <taskbar/task_small_browser_raised.xpm>
#include <taskbar/task_small_editor.xpm>
#include <taskbar/task_small_editor_armed.xpm>
#include <taskbar/task_small_editor_raised.xpm>
#include <taskbar/task_small_handle.xpm>
#include <taskbar/task_small_mail.xpm>
#include <taskbar/task_small_mail_armed.xpm>
#include <taskbar/task_small_mail_raised.xpm>
#include <taskbar/task_small_news.xpm>
#include <taskbar/task_small_news_armed.xpm>
#include <taskbar/task_small_news_raised.xpm>

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolBar pixmaps														*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <toolbar/tb_back.xpm>
#include <toolbar/tb_back_armed.xpm>
#include <toolbar/tb_back_raised.xpm>
#include <toolbar/tb_forward.xpm>
#include <toolbar/tb_forward_armed.xpm>
#include <toolbar/tb_forward_raised.xpm>
#include <toolbar/tb_home.xpm>
#include <toolbar/tb_home_armed.xpm>
#include <toolbar/tb_home_raised.xpm>
#include <toolbar/tb_loadimages.xpm>
#include <toolbar/tb_loadimages_armed.xpm>
#include <toolbar/tb_loadimages_raised.xpm>
#include <toolbar/tb_mixsecurity.xpm>
#include <toolbar/tb_mixsecurity_armed.xpm>
#include <toolbar/tb_mixsecurity_raised.xpm>
#include <toolbar/tb_places.xpm>
#include <toolbar/tb_places_armed.xpm>
#include <toolbar/tb_places_raised.xpm>
#include <toolbar/tb_print.xpm>
#include <toolbar/tb_print_armed.xpm>
#include <toolbar/tb_print_raised.xpm>
#include <toolbar/tb_reload.xpm>
#include <toolbar/tb_reload_armed.xpm>
#include <toolbar/tb_reload_raised.xpm>
#include <toolbar/tb_search.xpm>
#include <toolbar/tb_search_armed.xpm>
#include <toolbar/tb_search_raised.xpm>
#include <toolbar/tb_secure.xpm>
#include <toolbar/tb_secure_armed.xpm>
#include <toolbar/tb_secure_raised.xpm>
#include <toolbar/tb_stop.xpm>
#include <toolbar/tb_stop_armed.xpm>
#include <toolbar/tb_stop_raised.xpm>
#include <toolbar/tb_unsecure.xpm>
#include <toolbar/tb_unsecure_armed.xpm>
#include <toolbar/tb_unsecure_raised.xpm>

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolBox pixmaps														*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <toolbox/dtb_bottom.xpm>
#include <toolbox/dtb_bottom_raised.xpm>
#include <toolbox/dtb_horizontal.xpm>
#include <toolbox/dtb_horizontal_raised.xpm>
#include <toolbox/dtb_left.xpm>
#include <toolbox/dtb_left_raised.xpm>
#include <toolbox/dtb_right.xpm>
#include <toolbox/dtb_right_raised.xpm>
#include <toolbox/dtb_top.xpm>
#include <toolbox/dtb_top_raised.xpm>
#include <toolbox/dtb_vertical.xpm>
#include <toolbox/dtb_vertical_raised.xpm>

/*----------------------------------------------------------------------*/
/*																		*/
/* Proxy pixmaps														*/
/*																		*/
/*----------------------------------------------------------------------*/
#include <proxy/proxy.xpm>
#include <proxy/proxy_raised.xpm>

#define GR_ITEM(n,d) \
{ n , d , 0 , 0 },

static XfeGraphicRec _gr_data[] =
{
	GR_ITEM("task_browser",					task_browser)
	GR_ITEM("task_browser_armed",			task_browser_armed)
	GR_ITEM("task_browser_raised",			task_browser_raised)
	GR_ITEM("task_editor",					task_editor)
	GR_ITEM("task_editor_armed",			task_editor_armed)
	GR_ITEM("task_editor_raised",			task_editor_raised)
	GR_ITEM("task_mail",					task_mail)
	GR_ITEM("task_mail_armed",				task_mail_armed)
	GR_ITEM("task_mail_raised",				task_mail_raised)
	GR_ITEM("task_news",					task_news)
	GR_ITEM("task_news_armed",				task_news_armed)
	GR_ITEM("task_news_raised",				task_news_raised)
	GR_ITEM("task_small_browser",			task_small_browser)
	GR_ITEM("task_small_browser_armed",		task_small_browser_armed)
	GR_ITEM("task_small_browser_raised",	task_small_browser_raised)
	GR_ITEM("task_small_editor",			task_small_editor)
	GR_ITEM("task_small_editor_armed",		task_small_editor_armed)
	GR_ITEM("task_small_editor_raised",		task_small_editor_raised)
	GR_ITEM("task_small_handle",			task_small_handle)
	GR_ITEM("task_small_mail",				task_small_mail)
	GR_ITEM("task_small_mail_armed",		task_small_mail_armed)
	GR_ITEM("task_small_mail_raised",		task_small_mail_raised)
	GR_ITEM("task_small_news",				task_small_news)
	GR_ITEM("task_small_news_armed",		task_small_news_armed)
	GR_ITEM("task_small_news_raised",		task_small_news_raised)

	GR_ITEM("tb_back",						tb_back)
	GR_ITEM("tb_back_armed",				tb_back_armed)
	GR_ITEM("tb_back_raised",				tb_back_raised)
	GR_ITEM("tb_forward",					tb_forward)
	GR_ITEM("tb_forward_armed",				tb_forward_armed)
	GR_ITEM("tb_forward_raised",			tb_forward_raised)
	GR_ITEM("tb_home",						tb_home)
	GR_ITEM("tb_home_armed",				tb_home_armed)
	GR_ITEM("tb_home_raised",				tb_home_raised)
	GR_ITEM("tb_loadimages",				tb_loadimages)
	GR_ITEM("tb_loadimages_armed",			tb_loadimages_armed)
	GR_ITEM("tb_loadimages_raised",			tb_loadimages_raised)
	GR_ITEM("tb_mixsecurity",				tb_mixsecurity)
	GR_ITEM("tb_mixsecurity_armed",			tb_mixsecurity_armed)
	GR_ITEM("tb_mixsecurity_raised",		tb_mixsecurity_raised)
	GR_ITEM("tb_places",					tb_places)
	GR_ITEM("tb_places_armed",				tb_places_armed)
	GR_ITEM("tb_places_raised",				tb_places_raised)
	GR_ITEM("tb_print",						tb_print)
	GR_ITEM("tb_print_armed",				tb_print_armed)
	GR_ITEM("tb_print_raised",				tb_print_raised)
	GR_ITEM("tb_reload",					tb_reload)
	GR_ITEM("tb_reload_armed",				tb_reload_armed)
	GR_ITEM("tb_reload_raised",				tb_reload_raised)
	GR_ITEM("tb_search",					tb_search)
	GR_ITEM("tb_search_armed",				tb_search_armed)
	GR_ITEM("tb_search_raised",				tb_search_raised)
	GR_ITEM("tb_secure",					tb_secure)
	GR_ITEM("tb_secure_armed",				tb_secure_armed)
	GR_ITEM("tb_secure_raised",				tb_secure_raised)
	GR_ITEM("tb_stop",						tb_stop)
	GR_ITEM("tb_stop_armed",				tb_stop_armed)
	GR_ITEM("tb_stop_raised",				tb_stop_raised)
	GR_ITEM("tb_unsecure",					tb_unsecure)
	GR_ITEM("tb_unsecure_armed",			tb_unsecure_armed)
	GR_ITEM("tb_unsecure_raised",			tb_unsecure_raised)

	GR_ITEM("dtb_bottom",					dtb_bottom)
	GR_ITEM("dtb_bottom_raised",			dtb_bottom_raised)
	GR_ITEM("dtb_horizontal",				dtb_horizontal)
	GR_ITEM("dtb_horizontal_raised",		dtb_horizontal_raised)
	GR_ITEM("dtb_left",						dtb_left)
	GR_ITEM("dtb_left_raised",				dtb_left_raised)
	GR_ITEM("dtb_right",					dtb_right)
	GR_ITEM("dtb_right_raised",				dtb_right_raised)
	GR_ITEM("dtb_top",						dtb_top)
	GR_ITEM("dtb_top_raised",				dtb_top_raised)
	GR_ITEM("dtb_vertical",					dtb_vertical)
	GR_ITEM("dtb_vertical_raised",			dtb_vertical_raised)

	GR_ITEM("proxy",						proxy)
	GR_ITEM("proxy_raised",					proxy_raised)

	{ NULL },
};

/*----------------------------------------------------------------------*/
static XfeGraphic
GraphicFind(char * name)
{
	XfeGraphic	g = _gr_data;
	Cardinal	n = 0;

	while (g && (g->name != NULL))
	{
		if (strcmp(g->name,name) == 0)
		{
			return g;
		}

		n++;
		g++;
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
static void
GraphicAllocate(Widget w,XfeGraphic g)
{
	assert( g != NULL );
	assert( g->data != NULL );
	assert( XfeIsAlive(w) );

	assert( !XfePixmapGood(g->pixmap) );
	assert( !XfePixmapGood(g->mask) );

    XfeAllocatePixmapFromData(g->data,
							  XtDisplay(w),
							  DefaultRootWindow(XtDisplay(w)),
							  XfeColormap(w),
							  40000,
							  XfeDepth(w),
							  XfeBackground(w),
							  &g->pixmap,
							  &g->mask);
	
	assert( XfePixmapGood(g->pixmap) );
}
/*----------------------------------------------------------------------*/
/* extern */ XfeGraphic
XfeGetGraphic(Widget w,char * name)
{
	XfeGraphic g = GraphicFind(name);

	if (g)
	{
		if (!XfePixmapGood(g->pixmap))
		{
			GraphicAllocate(w,g);
		}
	}
	else
	{
		fprintf(stderr,"Graphic '%s' not found\n",name);
	}

	return g;
}
/*----------------------------------------------------------------------*/
/* extern */ Pixmap
XfeGetPixmap(Widget w,char * name)
{
	XfeGraphic g = XfeGetGraphic(w,name);

	return g ? g->pixmap : XmUNSPECIFIED_PIXMAP;
}
/*----------------------------------------------------------------------*/
/* extern */ Pixmap
XfeGetMask(Widget w,char * name)
{
	XfeGraphic g = XfeGetGraphic(w,name);
	
	return g ? g->mask : XmUNSPECIFIED_PIXMAP;
}
/*----------------------------------------------------------------------*/
