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


#include "xp.h"
#include "xp_ncent.h"

/*  xp_ncent.c
 *  Contains various housekeeping dealing with the NavCenter
 *      in an XP layer.
 */

#ifdef MOZILLA_CLIENT

/*  Define this if you want frame children in the nav center.
#define WANTFRAMES
 */



static XP_List *xp_GlobalLastActiveContext = NULL;
static XP_List *xp_GlobalDockedNavCenters = NULL;
static XP_List *xp_GlobalNavCenters = NULL;
static XP_List *xp_GlobalNavCenterInfo = NULL;
static XP_List *xp_GlobalViewHTMLPanes = NULL;

/*  Various info that should be displayed in a NavCenter should
 *      the context have some nav centers to display in.
 */
typedef struct {
    MWContext *m_pContext;
    char *m_pUrl;
    XP_List *m_pSitemaps;
} NavCenterContextInfo;

/*  HT_Pane to MWContext association for tracking docked state.
 */
typedef struct {
    MWContext *m_pContext;
    HT_Pane m_htPane;
} DockedNavCenter;

/*  HT_View to MWContext association for loading HTML into
 *      a NavCenter pane.
 */
typedef struct  {
    HT_View m_htView;
    MWContext *m_pContext;
} ViewHTMLPane;

/*  Find filter for nav center contexts.
 */
static XP_Bool xp_canbenavcentertarget(MWContext *pCX)
{
    XP_Bool bRetval = FALSE;
    
    if(NULL == pCX) {
        /*  Invalid param.
         */
    }
    else if(MWContextBrowser != pCX->type) {
        /*  Only looking for browser contexts.
         */
    }
    else if(pCX->is_grid_cell) {
        /*  Grid cells not allowed
         */
    }
    else if(EDT_IS_EDITOR(pCX)) {
        /*  Editor contexts not allowed
         */
    }
    else if(pCX->name && 0 == XP_STRCASECMP(pCX->name, "Netscape_Netcaster_Drawer")) {
        /*  Netcaster windows not allowed
         */
    }
    else {
        bRetval = TRUE;
    }
    
    return(bRetval);
}

/*  Function to return last active context which fits nav center
 *      criteria.
 */
static MWContext *xp_lastactivenavcentercontext(void)
{
    return(XP_GetLastActiveContext(xp_canbenavcentertarget));
}

/*  Return nav center info for a context.
 *  Can return NULL.
 */
static NavCenterContextInfo *xp_getnavcenterinfo(MWContext *pContext)
{
    NavCenterContextInfo *pInfo = NULL;
    
    if(pContext) {
        XP_List *pTraverse = xp_GlobalNavCenterInfo;
        
        /*  Go through the global list and return the object if found.
         */
        while((pInfo = (NavCenterContextInfo *)XP_ListNextObject(pTraverse))) {
            if(pContext == pInfo->m_pContext) {
                break;
            }
            pInfo = NULL;
        }
    }
    
    return(pInfo);
}

/*  Clean out the structure except for context pointer
 */
static void xp_gutnavcenterinfo(NavCenterContextInfo *pInfo)
{
    if(pInfo) {
        if(pInfo->m_pUrl) {
            XP_FREE(pInfo->m_pUrl);
            pInfo->m_pUrl = NULL;
        }
        if(pInfo->m_pSitemaps) {
            XP_List *pTraverse = pInfo->m_pSitemaps;
            char *pTemp = NULL;
            while((pTemp = (char *)XP_ListNextObject(pTraverse))) {
                XP_FREE(pTemp);
            }
            XP_ListDestroy(pInfo->m_pSitemaps);
            pInfo->m_pSitemaps = NULL;
        }
    }
}

/*  Initialzie a nav center with various data.
 *  It should be emptied or uninitialized before doing this.
 *  Docked state, and active browser need to be set before
 *      doing this in order for proper operation.
 */
static void xp_initnavcenter(HT_Pane htPane, MWContext *pContext, XP_Bool bInitFlag)
{
    if(bInitFlag) {
        if(NULL == pContext) {
            pContext = XP_GetNavCenterContext(htPane);
        }
        if(pContext) {
            NavCenterContextInfo *pInfo = xp_getnavcenterinfo(pContext);
            if(pInfo && pInfo->m_pUrl) {
                HT_AddRelatedLinksFor(htPane, pInfo->m_pUrl);
                if(pInfo->m_pSitemaps) {
                    XP_List *pLoop = pInfo->m_pSitemaps;
                    char *pSitemap = NULL;
                    while((pSitemap = (char *)XP_ListNextObject(pLoop))) {
                        HT_AddSitemapFor(htPane, pInfo->m_pUrl, pSitemap, NULL);
                    }
                }
            }
#ifdef WANTFRAMES
            if(pContext->grid_children) {
                XP_List *pTraverse = pContext->grid_children;
                MWContext *pChild = NULL;
                while((pChild = (MWContext *)XP_ListNextObject(pTraverse))) {
                    xp_initnavcenter(htPane, pChild, bInitFlag);
                }
            }
#endif
        }
    }
}

/*  Uninitialize a nav center with current info.
 *  Use this before changing activation or docking state, or pass in
 *      the context to uninit from.
 */
static void xp_uninitnavcenter(HT_Pane htPane, MWContext *pContext, XP_Bool bInitFlag)
{
    if(bInitFlag) {
        if(NULL == pContext) {
            pContext = XP_GetNavCenterContext(htPane);
        }
        if(pContext) {
            NavCenterContextInfo *pInfo = xp_getnavcenterinfo(pContext);
            if(pInfo && pInfo->m_pUrl) {
                HT_ExitPage(htPane, pInfo->m_pUrl);
            }
#ifdef WANTFRAMES
            if(pContext->grid_children) {
                XP_List *pTraverse = pContext->grid_children;
                MWContext *pChild = NULL;
                while((pChild = (MWContext *)XP_ListNextObject(pTraverse))) {
                    xp_uninitnavcenter(htPane, pChild, bInitFlag);
                }
            }
#endif
        }
    }
}

/*  Call these to get a list of NavCenters which a context
 *      will effect.
 *  WARNING:    The list will not be updated should any of
 *      the data to which it refers changes.  The operation
 *      should be atomic and synchronous for best results.
 *  The object in the list is just a HT_Pane cast to void *.
 *  Caller must XP_ListDestroy return value when finished.
 *  Pass in NULL as the context to get a list of floating
 *      nav centers (this basically excludes docked nav
 *      centers).
 *  Could return NULL....
 */
static XP_List *xp_getnavcenterlist(MWContext *pContext)
{
    XP_List *pRetval = NULL;
    MWContext *pCX = XP_GetNonGridContext(pContext);
    
    if((pCX && MWContextBrowser == pCX->type) || NULL == pCX) {
        /*  Create the return value up front.
         *  This could be optimized to only be created when needed.
         */
        pRetval = XP_ListNew();
        if(pRetval) {
            XP_List *pTraverse = xp_GlobalDockedNavCenters;
            DockedNavCenter *pDNC = NULL;
            
            /*  Is there a docked NavCenter?
             */
            if(pCX) {
                while((pDNC = (DockedNavCenter *)XP_ListNextObject(pTraverse))) {
                    if(pCX == pDNC->m_pContext) {
                        XP_ListAddObject(pRetval, (void *)pDNC->m_htPane);
                        break;
                    }
                }
            }
            
            /*  Add any nondocked NavCenters, if the context is the active one or
             *      there is no context.
             */
            if(NULL == pCX || xp_lastactivenavcentercontext() == pCX) {
                HT_Pane htFloater = NULL;
                pTraverse = xp_GlobalNavCenters;
                while((htFloater = (HT_Pane)XP_ListNextObject(pTraverse))) {
                    if(FALSE == XP_IsNavCenterDocked(htFloater)) {
                        XP_ListAddObject(pRetval, (void *)htFloater);
                    }
                }
            }
            
            /*  Get rid of the list if empty.
             */
            if(XP_ListIsEmpty(pRetval)) {
                XP_ListDestroy(pRetval);
                pRetval = NULL;
            }
        }
    }
    
    return(pRetval);
}

/*  The active top level browser window has changed.
 *  We need to update any floating nav centers.
 */
static void xp_unactivatebrowser(MWContext *pOld)
{
    if(pOld) {
        XP_List *pFloatingNavCenters = xp_getnavcenterlist(NULL);
        if(pFloatingNavCenters) {
            /*  Handle removal of any old data.
             */
            XP_List *pTraverse = pFloatingNavCenters;
            HT_Pane htFloater = NULL;
            while((htFloater = (HT_Pane)XP_ListNextObject(pTraverse))) {
                xp_uninitnavcenter(htFloater, pOld, TRUE);
                xp_initnavcenter(htFloater, NULL, TRUE);
            }
            
            XP_ListDestroy(pFloatingNavCenters);
            pFloatingNavCenters = NULL;
        }
    }
}

/*  Private docking routing, can override auto-initialization for
 *      custom optimization.
 */
static void xp_docknavcenter(HT_Pane htPane, MWContext *pCX, XP_Bool bInitFlag)
{
    pCX = XP_GetNonGridContext(pCX);
    if(pCX && MWContextBrowser == pCX->type && htPane) {
        DockedNavCenter *pNew = XP_NEW_ZAP(DockedNavCenter);
        
        /*  Need to uninit nav center from current info
         *      if a different context.
         */
        XP_Bool bInit = FALSE;
        if(pCX != XP_GetNavCenterContext(htPane)) {
            xp_uninitnavcenter(htPane, NULL, bInitFlag);
            bInit = TRUE;
        }
        
        if(pNew) {
            pNew->m_pContext = pCX;
            pNew->m_htPane = htPane;
            
            if(NULL == xp_GlobalDockedNavCenters) {
                xp_GlobalDockedNavCenters = XP_ListNew();
                if(NULL == xp_GlobalDockedNavCenters) {
                    XP_FREE(pNew);
                    pNew = NULL;
                }
            }
            if(xp_GlobalDockedNavCenters) {
                XP_ListAddObject(xp_GlobalDockedNavCenters, (void *)pNew);
            }
        }
        
        /*  nav center needs re-init.
         */
        if(bInit) {
            xp_initnavcenter(htPane, NULL, bInitFlag);
        }
    }
}

/*  Private undocking routing, can override auto-initialization for
 *      custom optimization.
 */
static void xp_undocknavcenter(HT_Pane htPane, XP_Bool bInitFlag)
{
    MWContext *pCX = XP_GetNavCenterContext(htPane);
    if(pCX && htPane && xp_GlobalDockedNavCenters) {
        XP_List *pTraverse = xp_GlobalDockedNavCenters;
        DockedNavCenter *pDNC = NULL;
        XP_Bool bInit = FALSE;
        
        /*  If this will make the nav center of a different context,
         *      we will need to uninit.
         */
        if(pCX != xp_lastactivenavcentercontext()) {
            xp_uninitnavcenter(htPane, NULL, bInitFlag);
            bInit = TRUE;
        }
        
        while((pDNC = (DockedNavCenter *)XP_ListNextObject(pTraverse))) {
            if(pCX == pDNC->m_pContext && htPane == pDNC->m_htPane) {
                /* pTraverse points beyond pDNC now, removing it will
                 *  not destroy the sanctity of the loop.
                 */
                XP_ListRemoveObject(xp_GlobalDockedNavCenters, (void *)pDNC);
                XP_FREE(pDNC);
                break;
            }
        }
        if(XP_ListIsEmpty(xp_GlobalDockedNavCenters)) {
            XP_ListDestroy(xp_GlobalDockedNavCenters);
            xp_GlobalDockedNavCenters = NULL;
        }
        
        if(bInit) {
            /*  Need to re-init the nav center.
             */
            xp_initnavcenter(htPane, NULL, bInitFlag);
        }
    }
}

/*  Remove any view to context association.
 */
void xp_removeviewassociation(HT_View htView, MWContext *pCX)
{
    XP_List *pTraverse = xp_GlobalViewHTMLPanes;
    ViewHTMLPane *pVHP = NULL;
    XP_Bool bRemove = FALSE;
    
    while((pVHP = (ViewHTMLPane *)XP_ListNextObject(pTraverse))) {
        if(pCX && pCX == pVHP->m_pContext) {
            bRemove = TRUE;
        }
        else if(htView && htView == pVHP->m_htView) {
            bRemove = TRUE;
        }
        
        if(bRemove) {
            XP_ListRemoveObject(xp_GlobalViewHTMLPanes, pVHP);
            XP_FREE(pVHP);
            pVHP = NULL;
            
            if(XP_ListIsEmpty(xp_GlobalViewHTMLPanes)) {
                XP_ListDestroy(xp_GlobalViewHTMLPanes);
                xp_GlobalViewHTMLPanes = NULL;
            }
            break;
        }
        bRemove = FALSE;
    }
}

/*  Return HTML Pane context in the NavCenter.
 */
MWContext *xp_gethtmlpane(HT_View htView)
{
    MWContext *pRetval = NULL;
    if(htView) {
        XP_List *pTraverse = xp_GlobalViewHTMLPanes;
        ViewHTMLPane *pVHP = NULL;
        
        while((pVHP = (ViewHTMLPane *)XP_ListNextObject(pTraverse))) {
            if(htView == pVHP->m_htView) {
                pRetval = pVHP->m_pContext;
                break;
            }
        }
    }
    return(pRetval);
}

/*  Call this when a context becomes active.
 *  A simple list of the contexts is kept, effectively a stack
 *      of active contexts.
 */
void XP_SetLastActiveContext(MWContext *pCX)
{
    if(pCX) {
        if(!xp_GlobalLastActiveContext) {
            xp_GlobalLastActiveContext = XP_ListNew();
        }
        if(xp_GlobalLastActiveContext) {
            MWContext *pAfterSet = NULL;
            MWContext *pBeforeSet = NULL;
            
            pBeforeSet = xp_lastactivenavcentercontext();
            /*  Remove the context from the stack.
             *  It will be added at the top.
             */
            if(XP_ListPeekTopObject(xp_GlobalLastActiveContext) != pCX) {
                XP_ListRemoveObject(xp_GlobalLastActiveContext, (void *)pCX);
                XP_ListAddObject(xp_GlobalLastActiveContext, (void *)pCX);
            }
            pAfterSet = xp_lastactivenavcentercontext();
            
            /*  Did the top level browser change.
             *  If so, we need to notify floating nav centers.
             */
            if(pBeforeSet != pAfterSet) {
                xp_unactivatebrowser(pBeforeSet);
            }
        }
    }
}

/*  Call this to get the last active context.
 *  If specified, pCallMe will decide what is a valid return value
 *      and what is not.
 */
MWContext *XP_GetLastActiveContext(ContextMatch MatchCallback)
{
    XP_List *pTraverse = xp_GlobalLastActiveContext;
    MWContext *pCX = NULL;
    
    while((pCX = (MWContext *)XP_ListNextObject(pTraverse))) {
        if(MatchCallback) {
            if(MatchCallback(pCX)) {
                break;
            }
        }
        else {
            /*  No callback, really want last active.
             */
            break;
        }
        pCX = NULL;
    }
    
    return(pCX);
}

/*  Removes context from last active stack.
 *  Just call this before context destruction.
 */
void XP_RemoveContextFromLastActiveStack(MWContext *pCX)
{
    if(pCX && xp_GlobalLastActiveContext) {
        MWContext *pBeforeRemove = NULL;
        MWContext *pAfterRemove = NULL;
        
        pBeforeRemove = xp_lastactivenavcentercontext();
        XP_ListRemoveObject(xp_GlobalLastActiveContext, (void *)pCX);
        pAfterRemove = xp_lastactivenavcentercontext();
        if(XP_ListIsEmpty(xp_GlobalLastActiveContext)) {
            XP_ListDestroy(xp_GlobalLastActiveContext);
            xp_GlobalLastActiveContext = NULL;
        }
        
        /*  Did the top level browser change.
         *  If so, we need to notify floating nav centers.
         */
        if(pBeforeRemove != pAfterRemove) {
            xp_unactivatebrowser(pBeforeRemove);
        }
    }
}

/*  Inform us about a new NavCenter.
 *  Call this after creation of a NavCenter.
 *  The second parameter is only valid if created in a docked
 *      state.  Pass in NULL otherwise.
 */
void XP_RegisterNavCenter(HT_Pane htPane, MWContext *pDocked)
{
    if(htPane) {
        if(NULL == xp_GlobalNavCenters) {
            xp_GlobalNavCenters = XP_ListNew();
        }
        if(xp_GlobalNavCenters) {
            XP_ListAddObject(xp_GlobalNavCenters, (void *)htPane);
        }
        
        /*  Handle creation in a docked state.
         */
        if(pDocked) {
            xp_docknavcenter(htPane, pDocked, FALSE);
        }
        
        /*  Initialize it with whatever info.
         */
        xp_initnavcenter(htPane, NULL, TRUE);
    }
}

/*  Unregister a NavCenter.
 *  Call this when ready to delete the NavCenter.
 */
void XP_UnregisterNavCenter(HT_Pane htPane)
{
    if(htPane && xp_GlobalNavCenters) {
        /*  Remove whatever info it may be holding.
         */
        xp_uninitnavcenter(htPane, NULL, TRUE);
        
        /*  Can obviously no longer be docked.
         *  Have it perform no init, uninit.
         */
        if(XP_IsNavCenterDocked(htPane)) {
            xp_undocknavcenter(htPane, FALSE);
        }
        
        /*  List cleanup.
         */
        XP_ListRemoveObject(xp_GlobalNavCenters, (void *)htPane);
        if(XP_ListIsEmpty(xp_GlobalNavCenters)) {
            XP_ListDestroy(xp_GlobalNavCenters);
            xp_GlobalNavCenters = NULL;
        }
    }
}

/*  Call this when docking a NavCenter to a top level window.
 *  This includes creating a NavCenter in a docked state, or
 *      docking a nav center when it was already docked in
 *      a different window.
 */
void XP_DockNavCenter(HT_Pane htPane, MWContext *pCX)
{
    XP_Bool bPrevDock = FALSE;
    
    if(XP_IsNavCenterDocked(htPane)) {
        bPrevDock = TRUE;
    }
    
    if(FALSE == bPrevDock) {
        xp_docknavcenter(htPane, pCX, TRUE);
    }
    else {
        /*  Previously already docked.
         *  Avoid a redock to some context scenario.
         *  Optimize auto init/uninit.
         */
        MWContext *pOldCX = XP_GetNavCenterContext(htPane);
        if(pOldCX != pCX) {
            xp_uninitnavcenter(htPane, NULL, TRUE);
            xp_undocknavcenter(htPane, FALSE);
            xp_docknavcenter(htPane, pCX, FALSE);
            xp_initnavcenter(htPane, NULL, TRUE);
        }
    }
}

/*  Call this when undocking a NavCenter from a top level window.
 */
void XP_UndockNavCenter(HT_Pane htPane)
{
    xp_undocknavcenter(htPane, TRUE);
}

/*  Returns wether or not a NavCenter is docked.
 *  This basically allows you to tell the difference between a docked
 *      or last active window return value from XP_GetNavCenterContext.
 */
XP_Bool XP_IsNavCenterDocked(HT_Pane htPane)
{
    XP_Bool bRetval = FALSE;
    XP_List *pTraverse = xp_GlobalDockedNavCenters;
    DockedNavCenter *pDNC = NULL;
    
    /*  Look through docked list.
     */
    while((pDNC = (DockedNavCenter *)XP_ListNextObject(pTraverse))) {
        if(htPane == pDNC->m_htPane) {
            bRetval = TRUE;
            break;
        }
    }
    
    return(bRetval);
}

/*  Call this to determine the context which a NavCenter operation
 *      will target.
 *  If not docked, picks last active browser window.
 *  Could return NULL....
 */
MWContext *XP_GetNavCenterContext(HT_Pane htPane)
{
    MWContext *pRetval = NULL;
    XP_List *pTraverse = xp_GlobalDockedNavCenters;
    DockedNavCenter *pDNC = NULL;
    
    /*  Look through docked list first.
     */
    while((pDNC = (DockedNavCenter *)XP_ListNextObject(pTraverse))) {
        if(htPane == pDNC->m_htPane) {
            pRetval = pDNC->m_pContext;
            break;
        }
    }
    
    /*  Use last active top level browser window.
     */
    if(NULL == pRetval) {
        pRetval = xp_lastactivenavcentercontext();
    }
    
    return(pRetval);
}

/*  Call this when visiting a new page for a context.
 */
void XP_SetNavCenterUrl(MWContext *pContext, char *pUrl)
{
    if(pUrl && pContext) {
        MWContext *pBrowser = XP_GetNonGridContext(pContext);
#ifndef WANTFRAMES
        if(pBrowser != pContext) {
            return;
        }
#endif
        if(MWContextBrowser == pBrowser->type) {
            NavCenterContextInfo *pInfo = xp_getnavcenterinfo(pContext);
            if(NULL == pInfo) {
                if(NULL == xp_GlobalNavCenterInfo) {
                    xp_GlobalNavCenterInfo = XP_ListNew();
                }
                if(xp_GlobalNavCenterInfo) {
                    pInfo = XP_NEW_ZAP(NavCenterContextInfo);
                    pInfo->m_pContext = pContext;
                    XP_ListAddObject(xp_GlobalNavCenterInfo, (void *)pInfo);
                }
            }
            if(pInfo) {
                XP_Bool bChangedUrls = FALSE;
                
                if(pInfo->m_pUrl) {
                    if(XP_STRCMP(pUrl, pInfo->m_pUrl)) {
                        bChangedUrls = TRUE;
                    }
                }
                else {
                    bChangedUrls = TRUE;
                }
                
                if(bChangedUrls) {
                    /*  Blow away the current nav center info and gut
                     *      our info struct.
                     */
                    XP_List *pCenters = xp_getnavcenterlist(pContext);
                    if(pCenters) {
                        XP_List *pTraverse = pCenters;
                        HT_Pane htPane = NULL;
                        while((htPane = (HT_Pane)XP_ListNextObject(pTraverse))) {
                            xp_uninitnavcenter(htPane, pContext, TRUE);
                        }
                    }
                    xp_gutnavcenterinfo(pInfo);
                    
                    /*  Copy over the url.
                     */
                    pInfo->m_pUrl = XP_STRDUP(pUrl);
                    
                    /*  Update any watching nav centers.
                     */
                    if(pCenters) {
                        XP_List *pTraverse = pCenters;
                        HT_Pane htPane = NULL;
                        while((htPane = (HT_Pane)XP_ListNextObject(pTraverse))) {
                            xp_initnavcenter(htPane, pContext, TRUE);
                        }
                        XP_ListDestroy(pCenters);
                        pCenters = NULL;
                    }
                }
            }
        }
    }
}

/*  Add a sitemap for a context.
 *  The url has to be set for a context before this will work.
 */
void XP_AddNavCenterSitemap(MWContext *pContext, char *pSitemap, char* name)
{
    if(pSitemap && pContext) {
        MWContext *pBrowser = XP_GetNonGridContext(pContext);
#ifndef WANTFRAMES
        if(pBrowser != pContext) {
            return;
        }
#endif
        if(MWContextBrowser == pBrowser->type) {
            NavCenterContextInfo *pInfo = xp_getnavcenterinfo(pContext);
            if(pInfo && pInfo->m_pUrl) {
                XP_Bool bDuplicate = FALSE;
                
                if(pInfo->m_pSitemaps) {
                    XP_List *pTraverse = pInfo->m_pSitemaps;
                    char *pCheck = NULL;
                    while((pCheck = (char *)XP_ListNextObject(pTraverse))) {
                        if(0 == XP_STRCMP(pCheck, pSitemap)) {
                            bDuplicate = TRUE;
                        }
                    }
                }
                else {
                    /*  Create the list, first time.
                     */
                    pInfo->m_pSitemaps = XP_ListNew();
                }
                
                if(FALSE == bDuplicate && pInfo->m_pSitemaps) {
                    char *pDup = XP_STRDUP(pSitemap);
                    /*  Add entry to list.
                     */
                    if(pDup) {
                        XP_ListAddObject(pInfo->m_pSitemaps, (void *)pDup);
                    }
                }
                
                if(FALSE == bDuplicate) {
                    /*  Display entry in various nav centers.
                     */
                    XP_List *pCenters = xp_getnavcenterlist(pContext);
                    if(pCenters) {
                        XP_List *pTraverse = pCenters;
                        HT_Pane htPane = NULL;
                        while((htPane = (HT_Pane)XP_ListNextObject(pTraverse))) {
                            HT_AddSitemapFor(htPane, pInfo->m_pUrl, pSitemap, name);
                        }
                        XP_ListDestroy(pCenters);
                        pCenters = NULL;
                    }
                }
            }
        }
    }
}

/*  Call this to association an HTML Pane MWContext to a NavCenter view.
 */
void XP_RegisterViewHTMLPane(HT_View htView, MWContext *pContext)
{
    if(htView && pContext) {
        ViewHTMLPane *pNew = XP_NEW_ZAP(ViewHTMLPane);
        if(pNew) {
            pNew->m_htView = htView;
            pNew->m_pContext = pContext;
            
            if(NULL == xp_GlobalViewHTMLPanes) {
                xp_GlobalViewHTMLPanes = XP_ListNew();
                if(NULL == xp_GlobalViewHTMLPanes) {
                    XP_FREE(pNew);
                    pNew = NULL;
                }
            }
            if(xp_GlobalViewHTMLPanes) {
                XP_ListAddObject(xp_GlobalViewHTMLPanes, (void *)pNew);
            }
        }
    }
}

/*  RDF HT Backend calls this to load content into the HTML view
 *      of a NavCenter HT_View.
 */
int XP_GetURLForView(HT_View htView, char *pAddress)
{
    int iRetval = MK_NO_ACTION;
    if(htView && pAddress) {
        MWContext *pHTMLPane = xp_gethtmlpane(htView);
        if(pHTMLPane) {
            URL_Struct *pUrl = NET_CreateURLStruct(pAddress, NET_DONT_RELOAD);
            if(pUrl) {
                iRetval = FE_GetURL(pHTMLPane, pUrl);
            }
        }
    }
    return(iRetval);
}

/*  Call this to remove nav center info for a context.
 *  Just call this before context destruction.
 */
void XP_RemoveNavCenterInfo(MWContext *pContext)
{
    MWContext *pCX = XP_GetNonGridContext(pContext);
    if(pCX && MWContextBrowser == pCX->type) {
        XP_List *pNavCenters = xp_getnavcenterlist(pContext);
        NavCenterContextInfo *pInfo = xp_getnavcenterinfo(pContext);
        if(pNavCenters) {
            XP_List *pTraverse = pNavCenters;
            HT_Pane htPane = NULL;
            while((htPane = (HT_Pane)XP_ListNextObject(pTraverse))) {
                xp_uninitnavcenter(htPane, pContext, TRUE);
                if(pCX == pContext && XP_IsNavCenterDocked(htPane)) {
                    xp_undocknavcenter(htPane, FALSE);
                }
            }
            XP_ListDestroy(pNavCenters);
            pNavCenters = NULL;
        }
        
        if(pInfo) {
            XP_ListRemoveObject(xp_GlobalNavCenterInfo, (void *)pInfo);
            if(XP_ListIsEmpty(xp_GlobalNavCenterInfo)) {
                XP_ListDestroy(xp_GlobalNavCenterInfo);
                xp_GlobalNavCenterInfo = NULL;
            }
            xp_gutnavcenterinfo(pInfo);
            XP_FREE(pInfo);
            pInfo = NULL;
        }
    }
    
    /*  Don't use the top level non-grid context for this one,
     *      as it may have grid children. though itself not
     *      going away.
     */
    if(pContext) {
        xp_removeviewassociation(NULL, pContext);
    }
}

#endif /* MOZILLA_CLIENT */

