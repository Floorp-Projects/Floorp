/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com>
 *   Dharma Sirnapalli <dsirnapalli@netscape.com>
 *   Ashish Bhatt <ashishbhatt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// File Overview....
//
// These are QA test case implementations
//


#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "Tests.h"
#include "nsihistory.h"
#include "nsiwebnav.h"
#include "nsirequest.h"
#include "nsidirserv.h"
#include "domwindow.h"
#include "selection.h"
#include "nsProfile.h"
#include "nsIClipboardCmd.h"
#include "nsIObserServ.h"
#include "nsIFile.h"
#include "nsIWebBrow.h"
#include "nsIWebProg.h"
#include "nsIWebBrowFind.h"
#include "nsIEditSession.h"
#include "nsICommandMgr.h"
#include "nsICmdParams.h"
#include "QaUtils.h"
#include "nsIIOService.h"
#include "nsIChannelTests.h"
#include "nsIHttpChannelTests.h"
#include <stdio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

nsresult rv;

// Register message for FindDialog communication
static UINT WM_FINDMSG = ::RegisterWindowMessage(FINDMSGSTRING);

BEGIN_MESSAGE_MAP(CTests, CWnd)
	//{{AFX_MSG_MAP(CTests)
	ON_COMMAND(ID_TESTS_CHANGEURL, OnTestsChangeUrl)
	ON_COMMAND(ID_TESTS_GLOBALHISTORY, OnTestsGlobalHistory)
	ON_COMMAND(ID_TESTS_CREATEFILE, OnTestsCreateFile)
	ON_COMMAND(ID_TESTS_CREATEPROFILE, OnTestsCreateprofile)
	ON_COMMAND(ID_TESTS_ADDTOOLTIPLISTENER, OnTestsAddTooltipListener)
	ON_COMMAND(ID_TESTS_ADDWEBPROGLISTENER, OnTestsAddWebProgListener)
	ON_COMMAND(ID_TESTS_ADDHISTORYLISTENER, OnTestsAddHistoryListener)
	ON_COMMAND(ID_TESTS_REMOVEHISTORYLISTENER, OnTestsRemovehistorylistener)
	ON_COMMAND(ID_TESTS_ADDURICONTENTLISTENER_ADDFROMNSIWEBBROWSER, OnTestsAddUriContentListenerByWebBrowser)
	ON_COMMAND(ID_TESTS_ADDURICONTENTLISTENER_ADDFROMNSIURILOADER, OnTestsAddUriContentListenerByUriLoader)
	ON_COMMAND(ID_TESTS_ADDURICONTENTLISTENER_OPENURI, OnTestsAddUriContentListenerByOpenUri)
	ON_COMMAND(ID_TESTS_NSNEWCHANNEL, OnTestsNSNewChannelAndAsyncOpen)
	ON_COMMAND(ID_TESTS_NSIIOSERVICENEWURI, OnTestsIOServiceNewURI)
	ON_COMMAND(ID_TESTS_NSIPROTOCOLHANDLERNEWURI, OnTestsProtocolHandlerNewURI)
	ON_COMMAND(ID_TOOLS_REMOVEGHPAGE, OnToolsRemoveGHPage)
	ON_COMMAND(ID_TOOLS_REMOVEALLGH, OnToolsRemoveAllGH)
	ON_COMMAND(ID_TOOLS_VIEWLOGFILE, OnToolsViewLogfile)
	ON_COMMAND(ID_TOOLS_DELETELOGFILE, OnToolsDeleteLogfile)
	ON_COMMAND(ID_TOOLS_TESTYOURMETHOD, OnToolsTestYourMethod)
	ON_COMMAND(ID_TOOLS_TESTYOURMETHOD2, OnToolsTestYourMethod2)
	ON_COMMAND(ID_VERIFYBUGS_70228, OnVerifybugs70228)
	ON_COMMAND(ID_VERIFYBUGS_90195, OnVerifybugs90195)
	ON_COMMAND(ID_VERIFYBUGS_169617, OnVerifybugs169617)
	ON_COMMAND(ID_VERIFYBUGS_170274, OnVerifybugs170274)
	ON_COMMAND(ID_INTERFACES_NSIDIRECTORYSERVICE_INIT, OnInterfacesNsidirectoryservice)
	ON_COMMAND(ID_INTERFACES_NSIDIRECTORYSERVICE_REGISTERPROVIDER, OnInterfacesNsidirectoryservice)
	ON_COMMAND(ID_INTERFACES_NSIDIRECTORYSERVICE_RUNALLTESTS, OnInterfacesNsidirectoryservice)
	ON_COMMAND(ID_INTERFACES_NSIDIRECTORYSERVICE_UNREGISTERPROVIDER, OnInterfacesNsidirectoryservice)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_RUNALLTESTS, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETDOMDOCUMENT, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETFRAMES, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETNAME, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETPARENT, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETSCROLLBARS, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETSCROLLY, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETSCSOLLX, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETSELECTION, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SCROLLBY, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SCROLLBYLINES, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SCROLLBYPAGES, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SCROLLTO, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SIZETOCONTENT, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_GETTEXTZOOM, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_SETTEXTZOOM, OnInterfacesNsidomwindow)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_RUNALLTESTS, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETANCHORNODE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_ADDRANGE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_COLLAPSE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_COLLAPSETOEND, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_COLLAPSETOSTART, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_CONTAINSNODE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_DELETEFROMDOCUMENT, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_EXTEND, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETANCHOROFFSET, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETFOCUSNODE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETFOCUSOFFSET, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETISCOLLAPSED, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETRANGEAT, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_GETRANGECOUNT, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_REMOVEALLRANGES, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_REMOVERANGE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_SELECTALLCHILDREN, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_SELECTIONLANGUAGECHANGE, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSISELECTION_TOSTRING, OnInterfacesNsiselection)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_RUNALLTESTS, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_CLONEPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_CREATENEWPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_DELETEPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_GETCURRENTPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_GETPROFILECOUNT, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_GETPROFILELIST, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_PROFILEEXISTS, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_RENAMEPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_SETCURRENTPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSIPROFILE_SHUTDOWNCURRENTPROFILE, OnInterfacesNsiprofile)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_GETCOUNT, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_GETENTRYATINDEX, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_GETINDEX, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_GETMAXLENGTH, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_GETSHISTORYENUMERATOR, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_PURGEHISTORY, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_RUNALLTESTS, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_SETMAXLENGTH, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETISSUBFRAME, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETTITLE, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETURI, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_RUNALLTESTS, OnInterfacesNsishistory)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETCANGOBACK, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETCANGOFORWARD, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETCURRENTURI, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETREFERRINGURI, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETDOCUMENT, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GETSESSIONHISTORY, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GOBACK, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GOFORWARD, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_GOTOINDEX, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_LOADURI, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_RELOAD, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_RUNALLTESTS, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_SETSESSIONHISTORY, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSIWEBNAV_STOP, OnInterfacesNsiwebnav)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_RUNALLTESTS, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETORIGINALURI, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETORIGINALURI, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETURI, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETOWNER, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETOWNER, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETNOTIFICATIONS, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETNOTIFICATIONS, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETSECURITYINFO, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETCONTENTTYPE, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETCONTENTTYPE, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETCONTENTCHARSET, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETCONTENTCHARSET, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_SETCONTENTLENGTH, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_GETCONTENTLENGTH, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_OPEN, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSICHANNEL_ASYNCOPEN, OnInterfacesNsichannel)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_GETLOADFLAGS, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_GETLOADGROUP, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_GETNAME, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_GETSTATUS, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_ISPENDING, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_CANCEL, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_RESUME, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_SETLOADFLAGS, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_SETLOADGROUP, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_SUSPEND, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSIREQUEST_RUNALLTESTS, OnInterfacesNsirequest)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANCUTSELECTION, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANCOPYSELECTION, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANPASTE, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_COPYLINKLOCATION, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_COPYSELECTION, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_CUTSELECTION, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_PASTE, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_SELECTALL, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSICLIPBOARDCOMMANDS_SELECTNONE, OnInterfacesNsiclipboardcommands)
	ON_COMMAND(ID_INTERFACES_NSIOBSERVERSERVICE_ADDOBSERVERS, OnInterfacesNsiobserverservice)
	ON_COMMAND(ID_INTERFACES_NSIOBSERVERSERVICE_ENUMERATEOBSERVERS, OnInterfacesNsiobserverservice)
	ON_COMMAND(ID_INTERFACES_NSIOBSERVERSERVICE_NOTIFYOBSERVERS, OnInterfacesNsiobserverservice)
	ON_COMMAND(ID_INTERFACES_NSIOBSERVERSERVICE_REMOVEOBSERVERS, OnInterfacesNsiobserverservice)
	ON_COMMAND(ID_INTERFACES_NSIOBSERVERSERVICE_RUNALLTESTS, OnInterfacesNsiobserverservice)
	ON_COMMAND(ID_INTERFACES_NSIFILE_APPENDRELATICEPATH, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_COPYTO, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_CREATE, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_EXISTS, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_INITWITHPATH, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_MOVETO, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIFILE_RUNALLTESTS, OnInterfacesNsifile)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_RUNALLTESTS, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_ADDWEBBROWSERLISTENER, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_REMOVEWEBBROWSERLISTENER, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_GETCONTAINERWINDOW, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_SETCONTAINERWINDOW, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_GETPARENTURICONTENTLISTENER, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_SETPARENTURICONTENTLISTENER, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_GETCONTENTDOMWINDOW, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSER_NSIWBSETUPSETPROPERTY, OnInterfacesNsiwebbrowser)
	ON_COMMAND(ID_INTERFACES_NSIWEBPROGRESS_RUNALLTESTS, OnInterfacesNsiwebprogress)
	ON_COMMAND(ID_INTERFACES_NSIWEBPROGRESS_ADDPROGRESSLISTENER, OnInterfacesNsiwebprogress)
	ON_COMMAND(ID_INTERFACES_NSIWEBPROGRESS_REMOVEPROGRESSLISTENER, OnInterfacesNsiwebprogress)
	ON_COMMAND(ID_INTERFACES_NSIWEBPROGRESS_GETDOMWINDOW, OnInterfacesNsiwebprogress)
	ON_COMMAND(ID_INTERFACES_NSIWEBPROGRESS_ISLOADINGDOCUMENT, OnInterfacesNsiwebprogress)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_RUNALLTESTS, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHSTRINGTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHSTRINGTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_FINDNEXTTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETFINDBACKWARDSTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETFINDBACKWARDSTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETWRAPFINDTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETWRAPFINDTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETENTIREWORDTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETENTIREWORDTEST, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETMATCHCASE, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETMATCHCASE, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHFRAMES, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHFRAMES, OnInterfacesNsiwebbrowfind)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_RUNALLTESTS, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_INIT, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_MAKEWINDOWEDITABLE, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_WINDOWISEDITABLE, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_GETEDITORFORWINDOW, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_SETUPEDITORONWINDOW, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSIEDITINGSESSION_TEARDOWNEDITORONWINDOW, OnInterfacesNsieditingsession)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_RUNALLTESTS, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_ADDCOMMANDOBSERVER, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_REMOVECOMMANDOBSERVER, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_ISCOMMANDESUPPORTED, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_ISCOMMANDENABLED, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_GETCOMMANDSTATE, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDMANAGER_DOCOMMAND, OnInterfacesNsicommandmgr)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_RUNALLTESTS, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETVALUETYPE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETBOOLEANVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETLONGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETDOUBLEVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETSTRINGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETCSTRINGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETISUPPORTSVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETBOOLEANVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETLONGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETDOUBLEVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETSTRINGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETCSTRINGVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_SETISUPPORTSVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_REMOVEVALUE, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_HASMOREELEMENTS, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_FIRST, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSICOMMANDPARAMS_GETNEXT, OnInterfacesNsicmdparams)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_RUNALLTESTS ,OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_SETREQUESTMETHOD, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTMETHOD, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_SETREFERRER, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETREFERRER, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_SETREQUESTHEADER, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTHEADER, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_VISITREQUESTHEADERS, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_SETALLOWPIPELINING, OnInterfacesNsihttpchannel)	
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETALLOWPIPELINING, OnInterfacesNsihttpchannel)	
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_SETREDIRECTIONLIMIT, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETREDIRECTIONLIMIT, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETRESPONSESTATUS, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_NSIHTTPCHANNEL_GETRESPONSESTATUSTEXT, OnInterfacesNsihttpchannel)
	ON_COMMAND(ID_INTERFACES_RUNALLTESTCASES, OnInterfacesRunalltestcases)
	
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

//static declarations
//nsCOMPtr<nsIWebBrowser> CTests::qaWebBrowser;

//end static declarations

CTests::CTests(nsIWebBrowser* mWebBrowser,
			   nsIBaseWindow* mBaseWindow,
			   nsIWebNavigation* mWebNav,
			   CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBaseWindow = mBaseWindow;
	qaWebNav = mWebNav;
	qaBrowserImpl = mpBrowserImpl;
}

CTests::~CTests()
{

}


// *********************************************************
// depstein: Start QA test cases here
// *********************************************************

void CTests::OnTestsChangeUrl()
{
	if (!qaWebNav)
	{
		QAOutput("Web navigation object not found. Change URL test not performed.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin Change URL test.", 1);
		rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(myDialog.m_urlfield).get(),
								myDialog.m_flagvalue, nsnull,nsnull, nsnull);

	    RvTestResult(rv, "rv LoadURI() test", 1);
		FormatAndPrintOutput("The url = ", myDialog.m_urlfield, 2);
		FormatAndPrintOutput("The flag = ", myDialog.m_flagvalue, 1);
		QAOutput("End Change URL test.", 1);
	}
	else
		QAOutput("Change URL test not executed.", 1);
}

// *********************************************************

void CTests::OnTestsGlobalHistory()
{
	// create instance of myHistory object. Call's XPCOM
	// service manager to pass the contract ID.

	PRBool theRetVal = PR_FALSE;

	nsCOMPtr<nsIGlobalHistory> myHistory(do_GetService(NS_GLOBALHISTORY_CONTRACTID));

	if (!myHistory)
	{
		QAOutput("Couldn't find history object. No GH tests performed.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin IsVisited() and AddPage() tests.", 2);
		FormatAndPrintOutput("The history url = ", myDialog.m_urlfield, 1);

		// see if url is already in the GH file (pre-AddPage() test)
		rv = myHistory->IsVisited(myDialog.m_urlfield, &theRetVal);
	    RvTestResult(rv, "rv IsVisited() test", 1);
		FormatAndPrintOutput("The IsVisited() boolean return value = ", theRetVal, 1);

		if (theRetVal)
			QAOutput("URL has been visited. Won't execute AddPage().", 2);
		else
		{
			QAOutput("URL hasn't been visited. Will execute AddPage().", 2);

			// adds a url to the global history file
			rv = myHistory->AddPage(myDialog.m_urlfield);

			// prints addPage() results to output file
			if (NS_FAILED(rv))
			{
				QAOutput("Invalid results for AddPage(). Url not added. Test failed.", 1);
				return;
			}
			else
				QAOutput("Valid results for AddPage(). Url added. Test passed.", 1);

			// checks if url was visited (post-AddPage() test)
 			myHistory->IsVisited(myDialog.m_urlfield, &theRetVal);

			if (theRetVal)
				QAOutput("URL is visited; post-AddPage() test. IsVisited() test passed.", 1);
			else
				QAOutput("URL isn't visited; post-AddPage() test. IsVisited() test failed.", 1);
		}
		QAOutput("End IsVisited() and AddPage() tests.", 2);
	}
	else
		QAOutput("IsVisited() and AddPage() tests not executed.", 1);
}


// *********************************************************

void CTests::OnTestsCreateFile()
{
   	//nsresult rv;

	PRBool exists;
    nsCOMPtr<nsILocalFile> theTestFile(do_GetService(NS_LOCAL_FILE_CONTRACTID));

    if (!theTestFile)
	{
		QAOutput("File object doesn't exist. No File tests performed.", 2);
		return;
	}


	QAOutput("Start Create File test.", 2);

	rv = theTestFile->InitWithNativePath(NS_LITERAL_CSTRING("c:\\temp\\theFile.txt"));
	rv = theTestFile->Exists(&exists);
	QAOutput("File (theFile.txt) doesn't exist. We'll create it.\r\n", 1);

	rv = theTestFile->Create(nsIFile::NORMAL_FILE_TYPE, 0777);
	RvTestResult(rv, "File Create() test", 2);
}

// *********************************************************

void CTests::OnTestsCreateprofile()
{
    CProfilesDlg    myDialog;
    nsresult        rv;

	if (myDialog.DoModal() == IDOK)
    {
		nsCOMPtr<nsIProfile> theProfServ(do_GetService(NS_PROFILE_CONTRACTID,&rv));
		if (NS_FAILED(rv))
		{
		   QAOutput("Didn't get profile service. No profile tests performed.", 2);
		   return;
		}

	   QAOutput("Start Profile switch test.", 2);

	   QAOutput("Retrieved profile service.", 2);

       rv = theProfServ->SetCurrentProfile(myDialog.m_SelectedProfile.get());
	   RvTestResult(rv, "SetCurrentProfile() (profile switching) test", 2);

	   QAOutput("End Profile switch test.", 2);
    }
	else
	   QAOutput("Profile switch test not executed.", 2);
}

// *********************************************************

void CTests::OnTestsAddTooltipListener()
{
    nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsITooltipListener*, qaBrowserImpl)));
 
	rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsITooltipListener));
	RvTestResult(rv, "AddWebBrowserListener(). Add Tooltip Listener test", 2);
}

// *********************************************************

void CTests::OnTestsAddWebProgListener()
{
    nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, qaBrowserImpl)));
 
	rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
	RvTestResult(rv, "AddWebBrowserListener(). Add Web Prog Lstnr test", 2);
}

// *********************************************************

void CTests::OnTestsAddHistoryListener()
{
   // addSHistoryListener test

	nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsISHistoryListener*, qaBrowserImpl)));

	rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsISHistoryListener));
	RvTestResult(rv, "AddWebBrowserListener(). Add History Lstnr test", 2);
}

// *********************************************************

void CTests::OnTestsRemovehistorylistener()
{
  // RemoveSHistoryListener test

	nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsISHistoryListener*, qaBrowserImpl)));

	rv = qaWebBrowser->RemoveWebBrowserListener(weakling, NS_GET_IID(nsISHistoryListener));
	RvTestResult(rv, "RemoveWebBrowserListener(). Remove History Lstnr test", 2);
}

// *********************************************************

void CTests::OnTestsAddUriContentListenerByWebBrowser()
{
    nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsIURIContentListener*, qaBrowserImpl)));

    rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIURIContentListener));
	RvTestResult(rv, "AddWebBrowserListener(). add nsIURIContentListener test", 2);
}

// *********************************************************

void CTests::OnTestsAddUriContentListenerByUriLoader()
{
	nsCOMPtr<nsIURILoader> myLoader(do_GetService(NS_URI_LOADER_CONTRACTID,&rv));
	RvTestResult(rv, "nsIURILoader() object test", 1);

	if (!myLoader) {
		QAOutput("Didn't get urILoader object. test failed", 2);
		return;
	}

	nsCOMPtr<nsIURIContentListener> cntListener(NS_STATIC_CAST(nsIURIContentListener*, qaBrowserImpl));

	if (!cntListener) {
		QAOutput("Didn't get urIContentListener object. test failed", 2);
		return;
	}
	else {
		rv = myLoader->RegisterContentListener(cntListener);
		RvTestResult(rv, "nsIUriLoader->RegisterContentListener() test", 2);
	}
}

// *********************************************************

void CTests::OnTestsAddUriContentListenerByOpenUri()
{
	nsCOMPtr<nsIURILoader> myLoader(do_GetService(NS_URI_LOADER_CONTRACTID,&rv));
	RvTestResult(rv, "nsIURILoader() object test", 1);

	if (!myLoader) {
		QAOutput("Didn't get urILoader object. test failed", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		nsCAutoString theStr;

		theStr = myDialog.m_urlfield;
		rv = NS_NewURI(getter_AddRefs(theURI), theStr);
		RvTestResult(rv, "For OpenURI(): NS_NewURI() test", 1);
		FormatAndPrintOutput("theStr = ", theStr, 1);
		GetTheURI(theURI, 1);
		rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
		RvTestResult(rv, "For OpenURI(): NS_NewChannel() test", 1);
	}
	else {
		QAOutput("Didn't get a url. test failed", 2);
		return;
	}

	rv = myLoader->OpenURI(theChannel, PR_TRUE, qaBrowserImpl);
	RvTestResult(rv, "nsIUriLoader->OpenURI() test", 2);
}

// *********************************************************

void CTests::OnTestsNSNewChannelAndAsyncOpen()
{
	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsILoadGroup> theLoadGroup(do_CreateInstance(NS_LOADGROUP_CONTRACTID));

	if (myDialog.DoModal() == IDOK)
	{
		nsCAutoString theStr;

		theStr = myDialog.m_urlfield;

		rv = NS_NewURI(getter_AddRefs(theURI), theStr);
		RvTestResult(rv, "NS_NewURI() test", 2);
		rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
		RvTestResult(rv, "NS_NewChannel() test", 2);

		if (!theChannel)
		{
		   QAOutput("We didn't get the Channel for AsyncOpen(). Test failed.", 1);
		   return;
		}

		QAOutput("AynchOpen() test:", 2);

		// this calls nsIStreamListener::OnDataAvailable()
		CnsIChannelTests  *channelTests = new CnsIChannelTests(qaWebBrowser, qaBrowserImpl);
		channelTests->AsyncOpenTest(theChannel, 2);;
	}
}

// *********************************************************

void CTests::OnTestsIOServiceNewURI()
{
	nsCOMPtr<nsIIOService> ioService(do_GetIOService(&rv));
	if (!ioService) {
		QAOutput("We didn't get the IOService object.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		nsCAutoString theStr, retURI;

		theStr = myDialog.m_urlfield;

		rv = ioService->NewURI(theStr, nsnull, nsnull, getter_AddRefs(theURI));
		RvTestResult(rv, "ioService->NewURI() test", 2);

		if (!theURI)
			QAOutput("We didn't get the nsIURI object for IOService test.", 2);
		else {
			retURI = GetTheURI(theURI, 1);
			FormatAndPrintOutput("The ioService->NewURI() output uri = ", retURI, 2);
		}
	}
}

// *********************************************************

void CTests::OnTestsProtocolHandlerNewURI()
{
	nsCOMPtr<nsIIOService> ioService(do_GetIOService(&rv));

	if (!ioService) {
		QAOutput("We didn't get the IOService object.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		nsCAutoString theStr, retStr;
		nsCOMPtr<nsIURI> theOriginalURI;
		nsIURI *theReturnURI = nsnull;
		nsIChannel *theReturnChannel = nsnull;
		const char *theProtocol;

		nsCOMPtr<nsIProtocolHandler> protocolHandler;

		theStr = myDialog.m_urlfield;
		theProtocol = myDialog.m_protocolvalue;
		FormatAndPrintOutput("The protocol scheme = ", theProtocol, 1);
		rv = ioService->GetProtocolHandler(theProtocol, getter_AddRefs(protocolHandler));
		RvTestResult(rv, "ioService->GetProtocolHandler() test", 2);
		rv = qaWebNav->GetCurrentURI(getter_AddRefs(theOriginalURI));
		if (!protocolHandler) {
			QAOutput("We didn't get the protocolHandler object.", 2);
			return;
		}
		rv = protocolHandler->NewURI(theStr, nsnull, theOriginalURI, &theReturnURI);
		if (!theOriginalURI)
			QAOutput("We didn't get the original nsIURI object.", 2);
		else if (!theReturnURI)
			QAOutput("We didn't get the output nsIURI object.", 2);

		RvTestResult(rv, "protocolHandler->NewURI() test", 2);

	    retStr = GetTheURI(theReturnURI, 1);

	   // simple string compare to see if input & output URLs match
	    if (strcmp(myDialog.m_urlfield, retStr.get()) == 0)
		   QAOutput("The in/out URIs MATCH.", 1);
	    else
		   QAOutput("The in/out URIs don't MATCH.", 1);

		// other protocol handler tests:

		// GetScheme() test
		nsCAutoString theScheme;
		rv = protocolHandler->GetScheme(theScheme);
		RvTestResult(rv, "protocolHandler->GetScheme() test", 1);
		FormatAndPrintOutput("GetScheme() = ", theScheme, 1);

		// GetDefaultPort() test
		PRInt32 theDefaultPort;
		rv = protocolHandler->GetDefaultPort(&theDefaultPort);
		RvTestResult(rv, "protocolHandler->GetDefaultPort() test", 1);
		FormatAndPrintOutput("GetDefaultPort() = ", theDefaultPort, 1);

		// GetProtocolFlags() test
		PRUint32 theProtocolFlags;
		rv = protocolHandler->GetProtocolFlags(&theProtocolFlags);
		RvTestResult(rv, "protocolHandler->GetDefaultPort() test", 1);
		FormatAndPrintOutput("GetProtocolFlags() = ", theProtocolFlags, 1);

		// NewChannel() test
		rv = protocolHandler->NewChannel(theReturnURI, &theReturnChannel);
		RvTestResult(rv, "protocolHandler->NewChannel() test", 1);
		if (!theReturnChannel)
			QAOutput("We didn't get the returned nsIChannel object.", 1);


		// AllowPort() test
		PRBool retPort;
		rv = protocolHandler->AllowPort(theDefaultPort, "http", &retPort);
		RvTestResult(rv, "protocolHandler->AllowPort() test", 1);
		FormatAndPrintOutput("AllowPort() boolean = ", retPort, 1);

		
	}
}

// *********************************************************
// *********************************************************
//					TOOLS to help us

void CTests::OnToolsRemoveGHPage()

{
	PRBool theRetVal = PR_FALSE;
	nsCOMPtr<nsIGlobalHistory> myGHistory(do_GetService(NS_GLOBALHISTORY_CONTRACTID));

	if (!myGHistory)
	{
		QAOutput("Could not get the global history object.", 2);
		return;
	}

	nsCOMPtr<nsIBrowserHistory> myHistory = do_QueryInterface(myGHistory, &rv);
	if(NS_FAILED(rv)) {
		QAOutput("Could not get the history object.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin URL removal from the GH file.", 2);

		myGHistory->IsVisited(myDialog.m_urlfield, &theRetVal);
		if (theRetVal)
		{
			rv = myHistory->RemovePage(myDialog.m_urlfield);
			RvTestResult(rv, "RemovePage() test (url removal from GH file)", 2);
		}
		else
		{
			QAOutput("The URL wasn't in the GH file.\r\n", 1);
		}
		QAOutput("End URL removal from the GH file.", 2);
	}
	else
		QAOutput("URL removal from the GH file not executed.", 2);
}

void CTests::OnToolsRemoveAllGH()
{

	nsCOMPtr<nsIGlobalHistory> myGHistory(do_GetService(NS_GLOBALHISTORY_CONTRACTID));
	if (!myGHistory)
	{
		QAOutput("Could not get the global history object.", 2);
		return;
	}

	nsCOMPtr<nsIBrowserHistory> myHistory = do_QueryInterface(myGHistory, &rv);
	if(NS_FAILED(rv)) {
		QAOutput("Could not get the history object.", 2);
		return;
	}

	QAOutput("Begin removal of all pages from the GH file.", 2);

	rv = myHistory->RemoveAllPages();
	RvTestResult(rv, "removeAllPages().", 2);

	QAOutput("End removal of all pages from the GH file.", 2);
}

void CTests::OnToolsViewLogfile()
{
	char theURI[1024];

	CStdioFile myFile; 
    CFileException e; 
    CString strFileName = "c:\\temp\\TestOutput.txt"; 
	myFile.Open( strFileName, CStdioFile::modeCreate | CStdioFile::modeWrite
			   | CStdioFile::modeNoTruncate, &e );               
	myFile.Close();

	strcpy(theURI, "file://C|/temp/TestOutput.txt");
	rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(theURI).get(),
		 nsIWebNavigation::LOAD_FLAGS_NONE, nsnull,nsnull, nsnull);
}

void CTests::OnToolsDeleteLogfile()
{
	CStdioFile myFile; 
    CFileException e; 
    CString strFileName = "c:\\temp\\TestOutput.txt"; 
	myFile.Open( strFileName, CStdioFile::modeCreate | CStdioFile::modeWrite
			   | CStdioFile::modeNoTruncate, &e );       
	myFile.Close();

	nsCOMPtr<nsILocalFile> theOriginalFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

	rv = theOriginalFile->InitWithNativePath(NS_LITERAL_CSTRING("c:\\temp\\TestOutput.txt"));
    nsCOMPtr<nsIFile> theTestFile = do_QueryInterface(theOriginalFile);
	rv = theTestFile->Remove(PR_FALSE);
}

// ***********************************************************************

void CTests::OnToolsTestYourMethod()
{	
	// place your test code here
}

// ***********************************************************************

void CTests::OnToolsTestYourMethod2()
{
	// place your test code here
}

// ***********************************************************************
// ***************** Bug Verifications ******************
// ***********************************************************************

void CTests::OnVerifybugs70228()
{
	nsCOMPtr<nsIHelperAppLauncherDialog>
			myHALD(do_CreateInstance(NS_IHELPERAPPLAUNCHERDLG_CONTRACTID));
	if (!myHALD)
		QAOutput("Object not created. It should be. It's a component!", 2);
	else
		QAOutput("Object is created. It's a component!", 2);
}

void CTests::OnVerifybugs90195()
{
    nsWeakPtr weakling(
        do_GetWeakReference(NS_STATIC_CAST(nsITooltipListener*, qaBrowserImpl)));

    rv = qaWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsITooltipListener));
	RvTestResult(rv, "AddWebBrowserListener(). Add Tool Tip Lstnr test", 2);

/*	nsCOMPtr<nsITooltipTextProvider> oTooltipTextProvider = do_GetService(NS_TOOLTIPTEXTPROVIDER_CONTRACTID) ;
	if (!oTooltipTextProvider)
		AfxMEssageBox("Asdfadf");
*/
}

void CTests::OnVerifybugs169617()
{
	nsCOMPtr<nsIURILoader> myLoader(do_GetService(NS_URI_LOADER_CONTRACTID,&rv));
	nsCAutoString theStr;

	QAOutput("Verification for bug 169617!", 2);

	theStr = "file://C|/Program Files";
	rv = NS_NewURI(getter_AddRefs(theURI), theStr);
	RvTestResult(rv, "NS_NewURI() test for file url", 1);

	GetTheURI(theURI, 1);

	rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
	RvTestResult(rv, "NS_NewChannel() test for file url", 1);

	rv = myLoader->OpenURI(theChannel, PR_TRUE, qaBrowserImpl);
	RvTestResult(rv, "nsIUriLoader->OpenURI() test for file url", 2);

}

void CTests::OnVerifybugs170274()
{
	nsCOMPtr<nsIURILoader> myLoader(do_GetService(NS_URI_LOADER_CONTRACTID,&rv));
	nsCAutoString theStr;

	QAOutput("Verification for bug 170274!", 2);

	theStr = "data:text/plain;charset=iso-8859-7,%be%fg%be";
	rv = NS_NewURI(getter_AddRefs(theURI), theStr);
	RvTestResult(rv, "NS_NewURI() test for data url", 1);

	GetTheURI(theURI, 1);

	rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
	RvTestResult(rv, "NS_NewChannel() test for data url", 1);

	rv = myLoader->OpenURI(theChannel, PR_TRUE, qaBrowserImpl);
	RvTestResult(rv, "nsIUriLoader->OpenURI() test for data url", 2);
}

// ***********************************************************************

BOOL CTests::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
   // To handle Menu handlers add here. Don't have to do if not handling
   // menu handlers
	nCommandID = nID;

	return CWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CTests::OnInterfacesNsirequest()
{
	CNsIRequest oNsIRequest(qaWebBrowser,/*qaBaseWindow,qaWebNav,*/ qaBrowserImpl);
	oNsIRequest.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsidirectoryservice()
{
	CNsIDirectoryService oNsIDirectoryService;
	oNsIDirectoryService.StartTests(nCommandID);
}

void CTests::OnInterfacesNsidomwindow()
{
	CDomWindow oDomWindow(qaWebBrowser);
	oDomWindow.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiselection()
{
	CSelection oSelection(qaWebBrowser);
	oSelection.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiprofile()
{
	CProfile oProfile(qaWebBrowser);
	oProfile.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsishistory()
{
	CNsIHistory oHistory(qaWebNav);
	oHistory.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiwebnav()
{
	CNsIWebNav oWebNav(qaWebNav);
	oWebNav.OnStartTests(nCommandID);
}


void CTests::OnInterfacesNsiclipboardcommands()
{
	CNsIClipBoardCmd  oClipCmd(qaWebBrowser);
	oClipCmd.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiobserverservice()
{
	CnsIObserServ oObserv;
	oObserv.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsifile()
{
	CNsIFile oFile ;
	oFile.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiwebbrowser()
{
	CNsIWebBrowser oWebBrowser(qaWebBrowser, qaBrowserImpl);
	oWebBrowser.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiwebprogress()
{
	CnsiWebProg oWebProgress(qaWebBrowser, qaBrowserImpl);
	oWebProgress.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsiwebbrowfind()
{
	CNsIWebBrowFind oWebBrowFind(qaWebBrowser, qaBrowserImpl);
	oWebBrowFind.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsieditingsession()
{
	CnsIEditSession oEditSession(qaWebBrowser);
	oEditSession.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsicommandmgr()
{
	CnsICommandMgr oCommandMgr(qaWebBrowser);
	oCommandMgr.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsicmdparams()
{
	CnsICmdParams oCmdParams(qaWebBrowser);
	oCmdParams.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsichannel()
{
	CnsIChannelTests oChannelTests(qaWebBrowser, qaBrowserImpl);
	oChannelTests.OnStartTests(nCommandID);
}

void CTests::OnInterfacesNsihttpchannel()
{
	CnsIHttpChannelTests oHttpChannelTests(qaWebBrowser, qaBrowserImpl);
	oHttpChannelTests.OnStartTests(nCommandID);
}


//Run all interface test cases in automation

void CTests::OnInterfacesRunalltestcases() 
{
	CNsIFile oFile ;
	oFile.OnStartTests(ID_INTERFACES_NSIFILE_RUNALLTESTS);

	CNsIHistory oHistory(qaWebNav);
	oHistory.OnStartTests(ID_INTERFACES_NSISHISTORY_RUNALLTESTS);


	// Can only be run manually
//	CNsIWebNav oWebNav(qaWebNav);
//	oWebNav.OnStartTests(ID_INTERFACES_NSIWEBNAV_RUNALLTESTS);


	CnsIObserServ oObserv;
	oObserv.OnStartTests(ID_INTERFACES_NSIOBSERVERSERVICE_RUNALLTESTS);


	CNsIDirectoryService oNsIDirectoryService;
	oNsIDirectoryService.StartTests(ID_INTERFACES_NSIDIRECTORYSERVICE_RUNALLTESTS);


	CDomWindow oDomWindow(qaWebBrowser) ;
	oDomWindow.OnStartTests(ID_INTERFACES_NSIDOMWINDOW_RUNALLTESTS);


	// Can only be run manually
	//CSelection oSelection(qaWebBrowser);
	//oSelection.OnStartTests(ID_INTERFACES_NSISELECTION_RUNALLTESTS);


	CProfile oProfile(qaWebBrowser);
	oProfile.OnStartTests(ID_INTERFACES_NSIPROFILE_RUNALLTESTS);


	// Can only be run manually
	//CNsIClipBoardCmd  oClipCmd(qaWebBrowser);
	//oClipCmd.OnStartTests(nCommandID);


	CNsIRequest oNsIRequest(qaWebBrowser,/*qaBaseWindow,qaWebNav,*/ qaBrowserImpl);
	oNsIRequest.OnStartTests(ID_INTERFACES_NSIREQUEST_RUNALLTESTS);


	CNsIWebBrowser oWebBrowser(qaWebBrowser, qaBrowserImpl);
	oWebBrowser.OnStartTests(ID_INTERFACES_NSIWEBBROWSER_RUNALLTESTS);


	CnsiWebProg oWebProgress(qaWebBrowser, qaBrowserImpl);
	oWebProgress.OnStartTests(ID_INTERFACES_NSIWEBPROGRESS_RUNALLTESTS);


	CNsIWebBrowFind oWebBrowFind(qaWebBrowser, qaBrowserImpl);
	oWebBrowFind.OnStartTests(ID_INTERFACES_NSIWEBBROWSERFIND_RUNALLTESTS);


	CnsIEditSession oEditSession(qaWebBrowser);
	oEditSession.OnStartTests(ID_INTERFACES_NSIEDITINGSESSION_RUNALLTESTS);


	CnsICommandMgr oCommandMgr(qaWebBrowser);
	oCommandMgr.OnStartTests(ID_INTERFACES_NSICOMMANDMANAGER_RUNALLTESTS);


	CnsICmdParams oCmdParams(qaWebBrowser);
	oCmdParams.OnStartTests(ID_INTERFACES_NSICOMMANDPARAMS_RUNALLTESTS);

	CnsIChannelTests oChannelTests(qaWebBrowser, qaBrowserImpl);
	oChannelTests.OnStartTests(ID_INTERFACES_NSICHANNEL_RUNALLTESTS);

	CnsIHttpChannelTests oHttpChannelTests(qaWebBrowser, qaBrowserImpl);
	oHttpChannelTests.OnStartTests(ID_INTERFACES_NSIHTTPCHANNEL_RUNALLTESTS);
}
