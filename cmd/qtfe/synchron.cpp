/* $Id: synchron.cpp,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <qtimer.h>
#include <qapp.h>
#include "QtBrowserContext.h" // includes QtContext.h
#include "qtbind.h"
#include <qmsgbox.h>
#include "layers.h"
#include "DialogPool.h"
#include "shist.h"
//#include "contexts.h"
#include <qobjcoll.h>
#include <string.h>
#include <qstring.h>
#include <qpainter.h>
#include <qmainwindow.h>
#include <qstatusbar.h>



/* this file contains some support for synchronous URL loading.
 * It is taken from the xfe though we are still not sure wether
 * we need this.
 *
 * Should be moved into a class.....
 */



bool fe_IsContextLooping(MWContext *context);
bool fe_IsContextDestroyed(MWContext *context);
bool fe_IsContextProtected(MWContext *context);
void fe_UnProtectContext(MWContext *context);
void fe_ProtectContext(MWContext *context);
void fe_LowerSynchronousURLDialog (MWContext *context);
void fe_synchronous_url_exit (URL_Struct *url, int status,
			      MWContext *context);
void fe_RaiseSynchronousURLDialog (MWContext *context, QWidget* parent,
				   const char *title);
int fe_AwaitSynchronousURL( MWContext* context );
void fe_StartProgressGraph ( MWContext* context );
void fe_SetCursor ( MWContext* context, bool );
void fe_url_exit (URL_Struct *url, int status, MWContext *context);

int
QtContext::getSynchronousURL ( QWidget* widget_to_grab,
			       const char *title,
			       URL_Struct *url,
			       int output_format,
			       void *call_data )
{
#if 0// defined DEBUG_paul
    debug( "getSynchronousURL" );
#endif

    int status;
    url->fe_data = call_data;
    fe_RaiseSynchronousURLDialog( context, widget_to_grab, title );
    status = NET_GetURL( url, output_format, context,
			 fe_synchronous_url_exit );
    if( status < 0 )
	return status;
    else
	return fe_AwaitSynchronousURL( context );
}


int fe_AwaitSynchronousURL( MWContext* context )
{
    /* Loop dispatching X events until the dialog box goes down as a result
       of the exit routine being called (which may be a result of the Cancel
       button being hit. */
    int status;

    fe_ProtectContext( context );
    while( CONTEXT_DATA ( context )->synchronous_url_dialog ) {
	qApp->processOneEvent();
    }
    fe_UnProtectContext( context );
    status = CONTEXT_DATA( context )->synchronous_url_exit_status;
    if( fe_IsContextDestroyed( context ) ) {
	free( CONTEXT_DATA( context ) );
	free( context );
    }
    return (status);
}

/*
 * fe_ProtectContext()
 *
 * Sets the dont_free_context_memory variable in the CONTEXT_DATA. This
 * will prevent the memory for the context and CONTEXT_DATA to be freed
 * even if the context was destroyed.
 */
void
fe_ProtectContext(MWContext *context)
{
    unsigned char del = 0; // XmDO_NOTHING;

  if (!CONTEXT_DATA(context)->dont_free_context_memory)
    {
      /* This is the first person trying to protect this context.
       * Dont allow the user to destroy this context using the windowmanger
       * delete menu item.
       */
    //#warning Implement this. Kalle.
	//      XtVaGetValues(CONTEXT_WIDGET(context), XmNdeleteResponse, &del, 0);
      CONTEXT_DATA(context)->delete_response = del;
      //      XtVaSetValues(CONTEXT_WIDGET(context),
      //	    XmNdeleteResponse, XmDO_NOTHING, 0);
    }
  CONTEXT_DATA(context)->dont_free_context_memory++;
}

/*
 * fe_UnProtectContext()
 *
 * Undo what fe_ProtectContext() does. Unsets dont_free_context_memory
 * variable in the context_data.
 */
void
fe_UnProtectContext(MWContext *context)
{
  XP_ASSERT(CONTEXT_DATA(context)->dont_free_context_memory);
  if (CONTEXT_DATA(context)->dont_free_context_memory)
    {
      CONTEXT_DATA(context)->dont_free_context_memory--;
      if (!CONTEXT_DATA(context)->dont_free_context_memory) {
	/* This is the last person unprotecting this context.
	 * Set the delete_response to what it was before.
	 */
    //#warning Implement this. Kalle.
	  //	XtVaSetValues(CONTEXT_WIDGET(context), XmNdeleteResponse,
	  //      CONTEXT_DATA(context)->delete_response, 0);
      }
    }
}

/*
 * fe_IsContextProtected()
 *
 * Return the protection state of the context.
 */
bool
fe_IsContextProtected(MWContext *context)
{
  return (CONTEXT_DATA(context)->dont_free_context_memory);
}

/*
 * fe_IsContextDestroyed()
 *
 * Return if the context was destroyed. This is valid only for protected
 * contexts.
 */
bool
fe_IsContextDestroyed(MWContext *context)
{
  return (CONTEXT_DATA(context)->destroyed);
}

/* Recurses over children of context, returning True if any context is
   stoppable. */
bool
fe_IsContextLooping(MWContext *context)
{
    int i = 1;
    MWContext *child;

    if (!context)
        return false;

    if (CONTEXT_DATA(context)->looping_images_p)
        return true;

    while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
													 i++)))
        if (fe_IsContextLooping(child))
            return true;

    return false;
}


void
fe_LowerSynchronousURLDialog (MWContext *context)
{
  if (CONTEXT_DATA (context)->synchronous_url_dialog)
    {
      CONTEXT_DATA (context)->synchronous_url_dialog = 0;

      /* If this context was destroyed, do not proceed furthur */
      if (fe_IsContextDestroyed(context))
	return;

      assert (CONTEXT_DATA (context)->active_url_count > 0);
      if (CONTEXT_DATA (context)->active_url_count > 0)
	CONTEXT_DATA (context)->active_url_count--;
      if (CONTEXT_DATA (context)->active_url_count <= 0)
	FE_AllConnectionsComplete (context);
      fe_SetCursor (context, false);
    }
}

void
fe_synchronous_url_exit (URL_Struct *url, int status, MWContext *context)
{
  if (status == MK_CHANGING_CONTEXT)
    return;
  fe_LowerSynchronousURLDialog (context);
  if (status < 0 && url->error_msg)
    {
      FE_Alert (context, url->error_msg);
    }
  NET_FreeURLStruct (url);
  CONTEXT_DATA (context)->synchronous_url_exit_status = status;
}

void
fe_RaiseSynchronousURLDialog (MWContext *context, QWidget* parent,
			      const char *title)
{
  CONTEXT_DATA (context)->active_url_count++;
  if (CONTEXT_DATA (context)->active_url_count == 1)
    {
      CONTEXT_DATA (context)->clicking_blocked = true;
      fe_StartProgressGraph (context);
      fe_SetCursor (context, false);
    }

  CONTEXT_DATA (context)->synchronous_url_dialog = (QWidget*) ~0;
  CONTEXT_DATA (context)->synchronous_url_exit_status = 0;
}

void fe_StartProgressGraph ( MWContext* context )
{
    //    debug( "fe_StartProgressGraph %p", context );
}

void fe_SetCursor ( MWContext* context, bool huh)
{
    //    debug( "fe_SetCursor %p %s", context, huh ? "TRUE" : "FALSE" );
}


void
fe_url_exit (URL_Struct *url, int status, MWContext *context)
{
  XP_ASSERT (status == MK_INTERRUPTED ||
			 CONTEXT_DATA (context)->active_url_count > 0);

  if (CONTEXT_DATA (context)->active_url_count > 0)
    CONTEXT_DATA (context)->active_url_count--;

  /* We should be guarenteed that XFE_AllConnectionsComplete() will be called
     right after this, if active_url_count has just gone to 0. */
  if (status < 0 && url->error_msg)
    {
      FE_Alert (context, url->error_msg);
    }
#if defined(EDITOR) && defined(EDITOR_UI)
  /*
   *    Do stuff specific to the editor
   */
  if (context->type == MWContextEditor)
    //#warning Put this back in when I understand what does.
{}      //    fe_EditorGetUrlExitRoutine(context, url, status);
#endif
  if (status != MK_CHANGING_CONTEXT)
    {
      NET_FreeURLStruct (url);
    }
}
