/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#define M12N

#ifdef MOZILLA_CLIENT

/*
 * xp_cntxt.c
 * Contains various XP context related functions
 * Recomended when FE communicates with lib code in a complex way
 */

#include "xp.h"
#include "shist.h"
#include "glhist.h"
#include "ssl.h"
#include "libimg.h"             /* Image Lib public API. */
#include "libmocha.h"
#include "libevent.h"
#include "intl_csi.h"
#include "xp_ncent.h"

static XP_List * xp_GlobalContextList = NULL;
static int32 global_context_id = 0;


void
XP_InitializeContext(MWContext *context)
{
	if (context != NULL)
	{
		context->context_id = ++global_context_id;
      context->INTL_tag = INTL_TAG;
	}
}

/* WARNING ! the mac FE does not call this ! */
MWContext *
XP_NewContext(void)
{
	MWContext *context;

	context = XP_NEW_ZAP(MWContext);
	XP_ASSERT(NULL != context);
	if (NULL == context)
	{
		return NULL;
	}
	context->INTL_CSIInfo = INTL_CSICreate();
	if (NULL == context->INTL_CSIInfo)
	{
		XP_FREE(context);
		return NULL;
	}
	XP_InitializeContext(context);
	return context;
}
/* WARNING ! the mac FE does not call this ! */
void
XP_DeleteContext(MWContext *context)
{
  XP_ASSERT(context);
  INTL_CSIDestroy(context->INTL_CSIInfo);
  XP_FREE(context);
}

/* give access to the global context list to other code too.
 */
XP_List *XP_GetGlobalContextList(void)
{
	return(xp_GlobalContextList);
}

/* cx_AddChildContext
 * Adds child context to the parents children list
 * NULL-safe
 */
void cx_AddChildContext(MWContext *parent, MWContext * child)
{
	if ((parent == NULL) || (child == NULL))
		return;
	if (parent->grid_children == NULL)
		parent->grid_children = XP_ListNew();
	if (parent->grid_children == NULL)
		return;
	XP_ListAddObject(parent->grid_children, child);
}

/* cx_RemoveChildContext
 * Removes child context from its parent
 * NULL-safe
 */
void cx_RemoveChildContext(MWContext * child)
{
	if ((child == NULL) || (child->grid_parent == NULL))
		return;
	XP_ListRemoveObject(child->grid_parent->grid_children, child);
}

/*
 * if the passed context is in the global context list
 * TRUE is returned.  Otherwise false
 */
Bool XP_IsContextInList(MWContext *context)
{
    if (context == NULL)
        return FALSE;

    if (xp_GlobalContextList == NULL)
        return(FALSE);

    return((XP_ListFindObject(xp_GlobalContextList, context)
			? TRUE : FALSE));
}

/*
 * The passed context is added to the global context list
 */
void XP_AddContextToList(MWContext *context)
{
	if (context == NULL)
		return;

	if (xp_GlobalContextList == NULL)
		xp_GlobalContextList = XP_ListNew();

	if (xp_GlobalContextList == NULL)
		return;

	XP_ListRemoveObject(xp_GlobalContextList, context);	/* No dups */
	XP_ListAddObject(xp_GlobalContextList, context);
	cx_AddChildContext(context->grid_parent, context);
}

void XP_UpdateParentContext(MWContext * child)
{
	cx_AddChildContext(child->grid_parent, child);
}
/*
 * Removes the context from the context list
 * Notifies all its children
 */
void XP_RemoveContextFromList(MWContext *context)
{
	int howMany;
	int i;

	if (context == NULL)
		return;
	cx_RemoveChildContext(context);	/* Its parent does not point to it any more */
	/* Now take care of the children, this should not really happen */
	if (context->grid_children)
	{
		howMany = XP_ListCount(context->grid_children);
		for (i = 1; i <= howMany; i++)
		{
			MWContext * child;
			child = (MWContext *)XP_ListGetObjectNum(context->grid_children, 1);
			if (child)
				child->grid_parent = NULL;
			XP_ListRemoveObject(context->grid_children, child);
		}
		XP_ListDestroy(context->grid_children);
		context->grid_children = NULL;
	}
	XP_ListRemoveObject(xp_GlobalContextList, context);
	
	/*  Remove from last active stack.
	 *  Remove any nav center info about the context.
	 */
	XP_RemoveContextFromLastActiveStack(context);
	XP_RemoveNavCenterInfo(context);
}

/* the following was adapted from xp_FindNamedContextInChildren, but
simplified for simply matching pointers instead of names. */
Bool
XP_IsChildContext(MWContext* parent,  MWContext* child)
{
	int i;
	if (parent == child) {
		return TRUE;
	}
	if (parent->grid_children) {
		int count = XP_ListCount(parent->grid_children);
		for (i = 1; i <= count; i++) {
			MWContext* tchild =
				(MWContext*)XP_ListGetObjectNum(parent->grid_children, i);
			if (child == tchild) {
				return TRUE;
			}
			else{
				XP_Bool found = XP_IsChildContext(tchild, child);
				if (found) return found;
			}
		}
	}
	return FALSE;
}


MWContext*
xp_FindNamedContextInChildren(MWContext* self, char* name, MWContext* excludeChild)
{
	int i;
/*
	XP_TRACE(("Searching for %s: context %0x, name=%s, title=%s\n",
			  (name ? name : ""), self,
			  (self->name ? self->name : ""), (self->title ? self->title : "")));
*/
	if (self->name && XP_STRCMP(self->name, name) == 0) {
/*
		XP_TRACE(("Found %s context %0x name=%s\n", (name ? name : ""),
				  self, self->name));
*/
		return self;
	}
	if (self->grid_children) {
		int count = XP_ListCount(self->grid_children);
		for (i = 1; i <= count; i++) {
			MWContext* child =
				(MWContext*)XP_ListGetObjectNum(self->grid_children, i);
			if (child != excludeChild) {
				MWContext* found = xp_FindNamedContextInChildren(child, name, NULL);
				if (found) return found;
			}
		}
	}
	return NULL;
}

/*
 * Finds a context that should be loaded with the URL, given
 * a name and current (refering) context.
 *
 * If the context returned is not NULL, name is already assigned to context
 * structure. You should load the URL into this context.
 *
 * If you get back a NULL, you should create a new window
 *
 * Both context and current context can be null.
 * Once the grids are in, there should be some kind of a logic that searches
 * siblings first. 
 */

MWContext * XP_FindNamedContextInList(MWContext * context, char *name)
{
	int i;
	
	if ((name == NULL) || (xp_GlobalContextList == NULL))
		return context;

	/*
	 * Check for special magic window target names
	 */
	if (name[0] == '_')
	{
		if (XP_STRNCMP(name, "_self", 5) == 0)
		{
			return context;
		}
		else if (XP_STRNCMP(name, "_parent", 7) == 0)
		{
			if ((context)&&(context->grid_parent))
			{
				return context->grid_parent;
			}
			else
			{
				return context;
			}
		}
		else if (XP_STRNCMP(name, "_top", 4) == 0)
		{
			MWContext *top;

			top = context;
			while ((top)&&(top->grid_parent))
			{
				top = top->grid_parent;
			}
			return top;
		}
		else if (XP_STRNCMP(name, "_blank", 6) == 0)
		{
			return NULL;
		}
		/* else, search for the name, below */
	}
	
	{
		MWContext* cx = context;
		MWContext* found;
		if (context) {
			/* If our current context has the right name, go there */
			if (cx->name && 
				(XP_STRCMP(cx->name, name) == 0))
				return cx;
			found = xp_FindNamedContextInChildren(cx, name, NULL);
			if (found) return found;
			while (cx->is_grid_cell) {
				MWContext* parent = cx->grid_parent;
				found = xp_FindNamedContextInChildren(parent, name, cx);
				if (found) return found;
				cx = parent;
			}
		}

		/* otherwise, just get any other context */
		for (i=1; i<= XP_ListCount(xp_GlobalContextList); i++)
		{
			MWContext* compContext = (MWContext *)XP_ListGetObjectNum(xp_GlobalContextList, i);
			/* Only search other top-level contexts that aren't the one we just came from: */
			if (!compContext->is_grid_cell && compContext != cx) {
				found = xp_FindNamedContextInChildren(compContext, name, NULL);
				if (found) return found;
			}
		}
	}
	return NULL;
}


/*
 * Finds a context that should be loaded with the URL, given
 * a type and current (refering) context.  Return NULL if there is none.
 */
MWContext * XP_FindContextOfType (MWContext * context, MWContextType type)
{
	int i;

	/* The other types aren't "real" - they don't have windows.  (Actually,
	   neither do all of these, but it's damned useful to be able to find the
	   biff context...) */
	XP_ASSERT (type == MWContextBrowser ||
			   type == MWContextMail ||
			   type == MWContextNews ||
			   type == MWContextMessageComposition ||
			   type == MWContextBookmarks ||
			   type == MWContextAddressBook ||
			   type == MWContextBiff ||
			   type == MWContextMailMsg ||
			   type == MWContextNewsMsg ||
			   type == MWContextEditor ||
			   type == MWContextPane); 
	/* Added MWContextEditor, needed for bug 61630  */

	/* If our current context has the right type, go there */
	if (context && context->type == type)
		return context;
	
	/* otherwise, just get any other context */
	for (i=1; i<= XP_ListCount(xp_GlobalContextList); i++)
	{
		MWContext * compContext = (MWContext *)XP_ListGetObjectNum(xp_GlobalContextList, i);
		if (compContext->type == type)
			return compContext;
	}
	return NULL;
}

MWContext*
XP_FindSomeContext()
{
    MWContext* cx = XP_FindContextOfType(NULL, MWContextBrowser);
    if (!cx)
		cx = XP_FindContextOfType(NULL, MWContextMail);
    if (!cx)
		cx = XP_FindContextOfType(NULL, MWContextNews);
    if (!cx)
		cx = XP_FindContextOfType(NULL, MWContextPane);
#if 0	/* dp says don't do this */
    if (!cx)
		cx = fe_all_MWContexts->context;
#endif
    return cx;
}

/* Returns the first non-grid parent of a given context (can be the context
 * itself if it's not a grid cell). 
 */
MWContext *
XP_GetNonGridContext(MWContext *context)
{
    MWContext* ctxt = context;
    while (ctxt && ctxt->is_grid_cell)
	ctxt = ctxt->grid_parent;
    return ctxt;
}

/* FE should call XP_FindNamedAnchor before getting a URL 
 * that is going into an HTML window (output_format is FO_CACHE_AND_PRESENT
 *
 * if named anchor is found:
 * XP_FindNamedAnchor 
 *					- updates context history, and global history properly
 * if it is not found, we do not touch any arguments
 *
 * Usage:
 * In your GetURL routine
 * if (XP_FindNamedAnchor(context, url, &x, &y))
 *  {
 * 		SetDocPosition(x,y);
 *		if (url->history_num == 0)	// if you are not handling go back this way, do not do this
 *			SHIST_AddDocument( &fContext, SHIST_CreateHistoryEntry( url, he->title) );
 *		else
 *			SHIST_SetCurrent(&fContext->hist, url->history_num);
 *      SetURLTextField(url->address);
 *      SetURLTextFieldTitle( ... url->is_netsite ... );
 *		NET_FreeURLStruct(url);
 *      RegenerateHistoryMenuAndDialogBoxContents()
 *  }
 * else
 *		NET_GetURL
 */
Bool XP_FindNamedAnchor(MWContext * context, URL_Struct * url, 
							int32 *xpos, int32 *ypos)
{
	History_entry *he;
	
	if (!context)
		return FALSE;

	he = SHIST_GetCurrent (&context->hist);
	
	if (he
		&& he->address
		&& url
		&& url->address
		&& XP_STRCHR(url->address, '#') /* it must have a named hash */
		&& url->method == URL_GET_METHOD
		&& url->force_reload == NET_DONT_RELOAD
		&& LO_LocateNamedAnchor(context, url, xpos, ypos))
	{
		GH_UpdateGlobalHistory( url );
		/* don't do this - jwz. */
		/* NET_FreeURLStruct( url ); */
		return TRUE;
	}
	return FALSE;	
}

/* XP_RefreshAnchors
 * call it after loading a new URL to refresh all the anchors
 */
void XP_RefreshAnchors()
{
	int i;
	for (i=1; i<= XP_ListCount(xp_GlobalContextList); i++)
	{
		MWContext * compContext = (MWContext *)XP_ListGetObjectNum(xp_GlobalContextList, i);
		LO_RefreshAnchors(compContext);
	}
}

/* XP_InterruptContext
 * interrupts this context, and all the grid cells it encloses
 */
void XP_InterruptContext(MWContext * context)
{
	int i = 1;
	MWContext * child;

	if (context == NULL)
		return;

	/* Interrupt the children recursively first.
	 * This avoids the scenario where the context is destroyed during
	 *  an interrupt and we end up dereferencing a freed context.
	 * This also avoids crazy hacks like caching the grid_children
	 *  context list until after the interrupt, and then doing the
	 *  below loop.  That can also crash if the grid_children context
	 *  list is freed off....
	 * Bug fix 58770
	 */
	while(child = (MWContext *)XP_ListGetObjectNum(context->grid_children, i++))
		XP_InterruptContext(child);

	NET_InterruptWindow(context);
    if (context->img_cx)
        IL_InterruptContext(context->img_cx);
	ET_InterruptContext(context);
}

Bool XP_IsContextBusy(MWContext * context)
{
	int i = 1;
	MWContext * child;
	
	if (context == NULL)
		return FALSE;

	if (NET_AreThereActiveConnectionsForWindow(context))
		return TRUE;
	
	while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
													 i++)))
		if (XP_IsContextBusy(child))
			return TRUE;
	
	return FALSE;
}

Bool XP_IsContextStoppable(MWContext * context)
{
	int i = 1;
	MWContext * child;
	
	if (context == NULL)
		return FALSE;

	if (NET_AreThereActiveConnectionsForWindow(context))
		return TRUE;
	
	if (LM_IsActive(context))
		return TRUE;

	while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
													 i++)))
		if (XP_IsContextStoppable(child))
			return TRUE;
	
	return FALSE;
}

/*
 *	Will return the lowest security status of the context
 *		and it's children.
 *	Lowest means lowest in security, not in numberic value.
 *
 *	Possible returns are:
 *	SSL_SECURITY_STATUS_NOOPT
 *	SSL_SECURITY_STATUS_OFF
 *	SSL_SECURITY_STATUS_ON_HIGH
 *	SSL_SECURITY_STATUS_ON_LOW
 */
int XP_GetSecurityStatus(MWContext *pContext)	{
	History_entry *pHistEnt;
	MWContext *pChild;
	int iRetval, iIndex;

	/*
	 *	No context, no security.
	 */
	if(pContext == NULL)	{
		return(SSL_SECURITY_STATUS_NOOPT);
	}

	/*
	 *	Obtain the context's current history entry (it holds
	 *		security info).
	 */
	pHistEnt = SHIST_GetCurrent(&(pContext->hist));
	if(pHistEnt == NULL)	{
		/*
		 *	Nothing loaded, Confusion ensues here.
		 *	If we've a parent context, then our security
		 *		is a noopt (shouldn't be counted towards
		 *		the whole).
		 *	If we've no parent context, then our security
		 *		is off.
		 *	This allows grids to have empty panes, but to
		 *		still be secure if all grids which have
		 *		something loaded are secure.
		 */
		if(pContext->grid_parent == NULL)	{
			return(SSL_SECURITY_STATUS_OFF);
		}
		else	{
			return(SSL_SECURITY_STATUS_NOOPT);
		}
	}

	/*
	 *	Our security status.
	 */
	iRetval = pHistEnt->security_on;

	/*
	 *	If we're a grid parent, our security status
	 *		is a noopt (doesn't affect the whole).
	 *	This allows the parent grid document to be non
	 *		secure, and all the inner grids to be secure,
	 *		and therefore the document should still be
	 *		considered secure as a whole.
	 */
	if(XP_ListIsEmpty(pContext->grid_children))	{
		return(iRetval);
	}
	iRetval = SSL_SECURITY_STATUS_NOOPT;

	/*
	 *	Go through each child, getting it's security status.
	 *	We combine them all and return that value.
	 */
	iIndex = 1;
	while((pChild = (MWContext *)XP_ListGetObjectNum(
		pContext->grid_children, iIndex++)))	{

		switch(XP_GetSecurityStatus(pChild))	{
		case SSL_SECURITY_STATUS_NOOPT:
			/*
			 *	No definable security status.  Don't
			 *		change our current value.
			 */
			break;
		case SSL_SECURITY_STATUS_OFF:
			/*
			 *	No need in continuing if we're turning
			 *		off security altogether.
			 */
			return(SSL_SECURITY_STATUS_OFF);
			break;
		case SSL_SECURITY_STATUS_ON_LOW:
			/*
			 *	Change the return value to lower security.
			 */
			iRetval = SSL_SECURITY_STATUS_ON_LOW;
			break;
		case SSL_SECURITY_STATUS_ON_HIGH:
			/*
			 *	If we're currently noopt, then we should
			 *		change the security status to HIGH.
			 *	Otherwise, don't change the cumulative status
			 *		which could be lower than HIGH.
			 */
			if(iRetval == SSL_SECURITY_STATUS_NOOPT)	{
				iRetval = SSL_SECURITY_STATUS_ON_HIGH;
			}
			break;
#ifdef FORTEZZA
		case SSL_SECURITY_STATUS_FORTEZZA:
			/*
			 * If any fortezza, set to fortezza..
			 */
			iRetval = SSL_SECURITY_STATUS_FORTEZZA;
			break;
#endif
		default:
			/*
			 *	Shouldn't be here, unless new security status
			 *		introduced.
			 */
			XP_ASSERT(0);
			break;
		}
	}

	/*
	 *	Context is somewhat secure, either high or low.
	 *	This could also be noopt, meaning no definable security,
	 *		so turn off your UI as if SSL_SECURITY_STATUS_OFF.
	 *	We can't just change the return value of
	 *		SSL_SECURITY_STATUS_NOOPT to OFF because this
	 *		breaks the recursive correctness of this routine.
	 *	Handle it accordingly.
	 */
	return(iRetval);
}

/*
 *	Count the contexts of the said type.
 *	The second parameter is a flag indicating wether or not the contexts
 *		counted can have parent contexts (top level context switch).
 */
int XP_ContextCount(MWContextType cxType, XP_Bool bTopLevel)
{
	int iRetval = 0;
	int iTraverse;
	MWContext *pContext;

	/*
	 *	Loop through the contexts.
	 */
	for(iTraverse = 1; iTraverse <= XP_ListCount(xp_GlobalContextList); iTraverse++)	{
		pContext = (MWContext *)XP_ListGetObjectNum(xp_GlobalContextList, iTraverse);
		if(cxType == MWContextAny || pContext->type == cxType)	{
			/*
			 *	See if there's to be no parent.
			 */
			if(bTopLevel == FALSE || pContext->is_grid_cell == FALSE)	{
				iRetval++;
			}
		}
	}
	
	return(iRetval);
}

#endif /* MOZILLA_CLIENT */
