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

#include "stdafx.h"

#include "cmdparse.h"
#ifdef MOZ_MAIL_NEWS
#include "wfemsg.h"
#include "msgcom.h"
#include "addrfrm.h"
#endif /* MOZ_MAIL_NEWS */
#include "template.h"
#include "mainfrm.h"
#include "woohoo.h"

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

void WFE_LJ_StartDebugger(void);
#ifdef MOZ_MAIL_NEWS
extern "C" void WFE_StartCalendar();
#endif
//
// Parse the command line for component launch arguments
// Set bRemove to TRUE if you want to remove the switch from pszCommandLine
BOOL CNetscapeApp::ParseComponentArguments(char * pszCommandLine,BOOL bRemove)
{	//simply look for the switch and set to true if it exists.  We use it later on.


	if (IsRuntimeSwitch("-BROWSER",  pszCommandLine, bRemove)){
		m_bCreateBrowser = TRUE;

#ifdef MOZ_MAIL_NEWS
	}else	if (IsRuntimeSwitch("-FOLDERS", pszCommandLine, bRemove)){
				m_bCreateFolders = TRUE;

	}else	if (IsRuntimeSwitch("-NEWS", pszCommandLine,  bRemove)){
				m_bCreateNews = TRUE;
			
	}else	if (IsRuntimeSwitch("-NETPROFILE", pszCommandLine,  bRemove)){
				m_bNetworkProfile = TRUE;
			
	}else	if (IsRuntimeSwitch("-COMPOSE", pszCommandLine,  bRemove)){
				m_bCreateCompose = TRUE;
#endif /* MOZ_MAIL_NEWS */

#ifdef EDITOR
	}else	if (IsRuntimeSwitch("-EDIT", pszCommandLine,  bRemove)){
				m_bCreateEdit = TRUE;
#endif // EDITOR

#ifdef MOZ_MAIL_NEWS
	}else	if (IsRuntimeSwitch("-INBOX", pszCommandLine,  bRemove)){
				m_bCreateInbox = TRUE;

  // rhp - for MAPI startup
	}else	if (IsRuntimeSwitch("-MAPICLIENT", pszCommandLine,  bRemove)){  
				m_bCreateInboxMAPI = TRUE;
  // rhp - for MAPI startup

	}else	if (IsRuntimeSwitch("-NABAPI", pszCommandLine,  bRemove)){  
				m_bCreateNABWin = TRUE;
  // rhp - for Address Book API startup

	}else	if (IsRuntimeSwitch("-ADDRESS", pszCommandLine,  bRemove)){
				m_bCreateAddress = TRUE;

#endif /* MOZ_MAIL_NEWS */
	}else	if (IsRuntimeSwitch("-CALENDAR", pszCommandLine,  bRemove)){
				m_bCreateCalendar = TRUE;

	}else	if (IsRuntimeSwitch("-IMPORT", pszCommandLine,  bRemove)){
				m_bCreateLDIF_IMPORT = TRUE;

   	}else	if (IsRuntimeSwitch("-EXPORT", pszCommandLine,  bRemove)){
				m_bCreateLDIF_EXPORT = TRUE;

	}else	return FALSE; //nothing to do here!!

	if (strlen(pszCommandLine) > 1) 
    { 
		m_bHasArguments = TRUE;
    }

	return TRUE;
	
}


///////////////////////////////////////////////////////////////////////////////
//  METHOD: ImportExportLDIF  - Imports or exports the specified ldif or html
//                              address book file.
//  PARAMETERS: pBook         - A pointer to the the address book.
//              pszFileName   - A fully qualified path including file name.
//              action        - STARTUP_IMPORT or STARTUP_EXPORT defined in 
//                              netscape.h
//  CALLERS:    Initinstance(), LaunchComponentWindow()
///////////////////////////////////////////////////////////////////////////////
BOOL CNetscapeApp::ImportExportLDIF(ABook *pBook, char *pszFileName, int action)
{
#ifdef MOZ_MAIL_NEWS
    if (action == STARTUP_IMPORT)
    {
        AB_ImportFromFileNamed(pBook, pszFileName);
    }
    else if (action == STARTUP_EXPORT)
    { 
        AB_ExportToFileNamed(pBook, pszFileName);
    }
#endif /* MOZ_MAIL_NEWS */
    return FALSE;
}


//Used in OnDDECommand to check the DDE string.
//Takes a command line string and searches for the specified arguements
//Similar to ParseComponentArguments except it doesn't have option to remove argument
BOOL CNetscapeApp::ExistComponentArguments(char * pszCommandLine)
{	//simply look for the switch and set to true if it exists.  We use it later on.

  // This is not an #ifdef, just here to make the braces match for all combinations
  // of ifdefs below.
  if (0) {  

#ifdef MOZ_MAIL_NEWS
  } else if (strcasestr(pszCommandLine, "-INBOX" )){
    m_bCreateInbox = TRUE;

  // rhp - for MAPI startup
  } else if (strcasestr(pszCommandLine, "-MAPICLIENT" )){
    m_bCreateInboxMAPI = TRUE;
  // rhp - for MAPI startup

  }else	if (strcasestr(pszCommandLine, "-NABAPI" )) {
    m_bCreateNABWin = TRUE;
  // rhp - for Address Book API startup

  } else if (strcasestr(pszCommandLine, "-FOLDERS" )){
    m_bCreateFolders = TRUE;

  } else if (strcasestr(pszCommandLine, "-FOLDER" )){
    m_bCreateFolder = TRUE;

  } else if (strcasestr(pszCommandLine, "-NEWS" )){
    m_bCreateNews = TRUE;

  } else if (strcasestr(pszCommandLine, "-MAIL" )){
    m_bCreateMail = TRUE;

#endif /* MOZ_MAIL_NEWS */
  } else if (strcasestr(pszCommandLine, "-NETCASTER" )){
    m_bCreateNetcaster = TRUE;

  }else	if (strcasestr(pszCommandLine, "-CALENDAR" )){
    m_bCreateCalendar = TRUE;

#ifdef MOZ_MAIL_NEWS
  } else if (strcasestr(pszCommandLine, "-COMPOSE" )){
    m_bCreateCompose = TRUE;
#endif // MOZ_MAIL_NEWS

#ifdef EDITOR
  } else if (strcasestr(pszCommandLine,"-EDIT" )){
    m_bCreateEdit = TRUE;
#endif // EDITOR

#ifdef MOZ_MAIL_NEWS
  } else if (strcasestr(pszCommandLine,"-ADDRESS" )){
    m_bCreateAddress = TRUE;
#endif // MOZ_MAIL_NEWS

  } else if (strcasestr(pszCommandLine,"-IMPORT" )){
    m_bCreateLDIF_IMPORT = TRUE;

  } else if (strcasestr(pszCommandLine,"-EXPORT" )){
    m_bCreateLDIF_EXPORT = TRUE;

  // This *&#! stupid !@#$% has to go here! (above -browser)
  } else if (strcasestr(pszCommandLine,"-NEW_ACCOUNT" )){
    // Any second instance trying to go to Account Setup
    // is likely to have problems.  Setting m_bAccountSetup
    // will cause an error below.
    m_bAccountSetup = TRUE;
    m_bAlwaysDockTaskBar= TRUE;
    m_bAccountSetupStartupJava = TRUE;

  } else if (strcasestr(pszCommandLine,"-START_JAVA" )) {
    m_bAccountSetupStartupJava = TRUE;


  } else if (strcasestr(pszCommandLine,"-BROWSER" )){
    m_bCreateBrowser = TRUE;

  } else if (strcasestr(pszCommandLine,"-NEW_PROFILE" )){
    m_bCreateNewProfile = TRUE;
      
  } else if (strcasestr(pszCommandLine,"-PROFILE_MANAGER" )){
    m_bProfileManager = TRUE;

#if defined(OJI) || defined(JAVA)
  } else if (strcasestr(pszCommandLine,"-javadebug" )){
    m_bCreateJavaDebugAgent = TRUE;
#endif

  } else	{
    return FALSE; //nothing to do here!!
  }

  if (strlen(pszCommandLine) > 1) {
    m_bHasArguments = TRUE;
  }

  return TRUE;  
}

/**************************************************************************************
 CNetscapeApp::BuildCmdLineList                                  Introduce by AJ 1/9/97

 Purpose:   Build a token list from a given token string after first removing a 
            specified flag which is not to be included in the token list.
 
 Parameters: 
            const char* pszRemoveString:  Flag to be stripped from the string
			CStringList &rlstrList:       List to hold tokens pulled from the string
			char* pszCmdLine:             Command line to pull tokens from.

 Called by: Called from OnDDECommand to parse out command line arguments.

 Returns:   Fills rlstrList with any tokens found on the command line with the exception
            of the pszRemoveString parameter.  If pszRemoveString is NULL, then the
			string will be trunkated to the first starting '"' in the string.

 Expecting: pszCmdLine is expected to be passed a string in the form of 
            [cmdline("-FLAG message.txt @attach.txt")] for example.
***************************************************************************************/

BOOL CNetscapeApp::BuildCmdLineList(const char* pszRemoveString, CCmdParse &cmdParse, CStringList &rlstrList, char* pszCmdLine)
{
    char *pszTemp = NULL;
	char *pFirst = NULL;
	char *pEnd   = NULL;
	char *pszRemove = NULL;
    char *pFind   = NULL;
   	BOOL bError = FALSE; 

 
	StrAllocCopy(pszRemove,pszRemoveString);
	//make sure we are playing the same game
	pszCmdLine = _strupr(pszCmdLine);
	pszRemove = _strupr(pszRemove);

    pFind =  strstr(pszCmdLine, "CMDLINE");

	pFirst = strstr(pszCmdLine, pszRemove);
      
	if (!pFirst)
	{   //just in case no switch was given
		pFirst = strchr(pszCmdLine,'"');
	}
	else
	{   //jump over the flag since we don't want it on the list
     	pFirst += XP_STRLEN(pszRemove);
	}
		
	if (pFirst && (pEnd = strrchr(pFirst,'"')) && pFind)
	{   //parse out the arguments and put them on the list
		*pEnd = NULL;
		cmdParse.Init(pFirst,rlstrList);
		if(!(bError = cmdParse.ProcessCmdLine()))
		{
			AfxMessageBox(IDS_CMDLINE_ERROR3);	
		}
	}
    else if (pFirst && !pFind)
    {//The DDE command stuff was already parsed out
		cmdParse.Init(pFirst,rlstrList);
		if(!(bError = cmdParse.ProcessCmdLine()))
		{
			AfxMessageBox(IDS_CMDLINE_ERROR3);	
		}
    }

	//free the string we allocated to play the same game
	XP_FREE(pszRemove);
	return bError;
}

/***************************************************************************
*	CNetscapeApp::LaunchComponentWindow
*
*	PARAMETERS:	iStartupMode.  Tells us which component is to be launch.
*				It is initialized by SetStartupMode(..).
*	
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function launches the specified component window.  Called from
*		only from OnDDECommand(..).  Assumption is that we are the first 
*		instance of navigator and a second instance has requested us to do 
*		something.
*		
****************************************************************************/

void CNetscapeApp::LaunchComponentWindow(int iStartupMode, char *pszCmdLine)
{
	void	WFE_AddNewFrameToFrameList(CGenericFrame * pFrame);
	void    OpenDraftExit (URL_Struct *url_struct, int/*status*/,MWContext *pContext);

    CStringList lstrArgumentList;
	CCmdParse cmdParse;
	switch(iStartupMode)
	{
#ifdef MOZ_MAIL_NEWS
		case STARTUP_INBOX:
			WFE_MSGOpenInbox();
			break;

    // rhp - for MAPI startup
    case STARTUP_CLIENT_MAPI:
      {
        CGenericDoc *pDoc = (CGenericDoc *)theApp.m_ViewTmplate->OpenDocumentFile(NULL, FALSE);
        if (pDoc)
        {
          CFrameGlue *pFrame = (pDoc->GetContext())->GetFrame();
		      if(pFrame)
          {
        	  CFrameWnd *pFrameWnd = pFrame->GetFrameWnd();
            if (pFrameWnd)
            {
            extern void StoreMAPIFrameWnd(CFrameWnd *pFrameWnd);

              pFrameWnd->ShowWindow(SW_SHOWMINIMIZED);
              StoreMAPIFrameWnd(pFrameWnd);
            }
          } 
        }
      }
			break;
    // rhp - for MAPI startup

		case STARTUP_FOLDER:
			WFE_MSGOpenInbox();			// Will eventually open a specific folder
			break;

		case STARTUP_MAIL:
		//	CFolderFrame::Open();		//opens the collections window
		//  Per John's request we are opening the inbox for a -mail flag.
			WFE_MSGOpenInbox();

			break;
         
		case STARTUP_FOLDERS:
			WFE_MSGOpenFolders();		//opens the collections window
			break;					

 
		case STARTUP_COMPOSE:

             if (!BuildCmdLineList("-COMPOSE",cmdParse, lstrArgumentList,pszCmdLine) )
				return; //error occured!!

			if(!lstrArgumentList.GetCount())
				MSG_Mail(m_pBmContext);	// starts the mail composition window.  
			else
			{	//handle any extra arguments for this flag here.
				char *pTemp;
				URL_Struct* pURL;
				CString strUrl;
			    
				int nArgCount = lstrArgumentList.GetCount();
				if (nArgCount == 1)
				{//either a message or an attachment.  If it's an attachment it will be prefixed with an "@" symbol
					pTemp  = (char*) (const char*)lstrArgumentList.GetHead();
					if(*pTemp != '@' && *pTemp!='-') //decide whether it's a message or an attachment
					{//it's a message
						CFileStatus rStatus;
                        if(CFile::GetStatus(pTemp,rStatus))
						{
                            WFE_ConvertFile2Url(strUrl, pTemp);
							pURL = NET_CreateURLStruct(strUrl, NET_DONT_RELOAD);
							StrAllocCopy(pURL->content_type,"message/rfc822"); 
							NET_GetURL (pURL, FO_OPEN_DRAFT, m_pBmContext,
                                       OpenDraftExit);
							//Load it into the compose window
						}
						else
						{
							AfxMessageBox(IDS_CMDLINE_ERROR1);
							MSG_Mail(m_pBmContext);
							//were just going to act as if we couldn't read what they gave us and continue	
						}
					}
					else
					{//it's an attachment
						pTemp += 1;
						CFileStatus rStatus;
                        if(CFile::GetStatus(pTemp,rStatus))
						{
							if (!LaunchComposeAndAttach(lstrArgumentList))
							{
								AfxMessageBox(IDS_CMDLINE_ERROR3);
								MSG_Mail(m_pBmContext);
							}
	                        //Load it into the compose window
						}
						else
						{
							AfxMessageBox(IDS_CMDLINE_ERROR1);
							MSG_Mail(m_pBmContext); 
						}					
					}					
				}
				else
				{
					if (nArgCount == 2)
					{
						//it's a message and has an attachment(s)
						if (!LaunchComposeAndAttach(lstrArgumentList))
						{
							AfxMessageBox(IDS_CMDLINE_ERROR3);
							MSG_Mail(m_pBmContext);
						}
					}
					else
					{//command line is not in a recognizable format for this flag
						AfxMessageBox(IDS_CMDLINE_ERROR3);
						MSG_Mail(m_pBmContext); 
						//ERROR!!!
					}
				}
			}
			break;                      

		case STARTUP_ADDRESS:
			CAddrFrame::Open();
			break;

       case STARTUP_IMPORT:
            if (!BuildCmdLineList("-IMPORT",cmdParse, lstrArgumentList,pszCmdLine) )
				return; //error occured!!

			if(!lstrArgumentList.GetCount())
				return; //nothing on the list
            else
            {
				char *pTemp  = (char*) (const char*)lstrArgumentList.GetHead();
                ImportExportLDIF(m_pABook, pTemp, iStartupMode);   
            }
            return; //we don't want to fall through on this case

       case STARTUP_EXPORT:
            if (!BuildCmdLineList("-EXPORT",cmdParse, lstrArgumentList,pszCmdLine) )
				return; //error occured!!

			if(!lstrArgumentList.GetCount())
				return; //nothing on the list
            else
            {
				char *pTemp  = (char*) (const char*)lstrArgumentList.GetHead();
                ImportExportLDIF(m_pABook, pTemp, iStartupMode);
            }
            return;//we don't want to fall through on this case

		case STARTUP_NEWS:
			//starts the news and opens the default news server and group in split pain.
			WFE_MSGOpenNews();
			break; 

		case STARTUP_CALENDAR:
			WFE_StartCalendar();
			break; 
#endif // MOZ_MAIL_NEWS

#ifdef EDITOR			
		case STARTUP_EDITOR:
            RemoveDDESyntaxAndSwitch("-edit",pszCmdLine);
		    //we do this outside the switch
			break;
#endif /* EDITOR */         
        
		// Outside MOZ_MAIL_NEWS and EDITOR flags in case lite and retail ever happen together...
		case STARTUP_ACCOUNT_SETUP:
			// Account Setup can't DDE
			AfxMessageBox(IDS_CMDLINE_ERROR7); 
			return;

		case STARTUP_BROWSER:
            RemoveDDESyntaxAndSwitch("-browser",pszCmdLine);

			//we do this outside the switch
			break; 

#if defined(OJI) || defined(JAVA)
		case STARTUP_NETCASTER:
			FEU_OpenNetcaster() ;
			break;

		case STARTUP_JAVA_DEBUG_AGENT:
			WFE_LJ_StartDebugger();
			break;

		case STARTUP_JAVA:
			//we decided to ignore the -start_java flag on second instance
			//and just simply continue to start navigator
			break;
#endif

        case 0:
			{
			AfxMessageBox(IDS_CMDLINE_ERROR4);
			}
 			return;
			
 			
		default:
			break;
	}
				

  // rhp - added STARTUP_CLIENT_MAPI for starting MAPI
    if ((iStartupMode & STARTUP_BROWSER || iStartupMode & STARTUP_EDITOR || // if startup browser or editor
        !(iStartupMode & (STARTUP_JAVA_DEBUG_AGENT|STARTUP_BROWSER|STARTUP_NEWS|STARTUP_MAIL|STARTUP_ADDRESS
#ifdef MOZ_MAIL_NEWS
		|STARTUP_INBOX|STARTUP_COMPOSE|STARTUP_FOLDER|STARTUP_FOLDERS|STARTUP_NETCASTER|STARTUP_CALENDAR 
		|STARTUP_CLIENT_MAPI|STARTUP_CLIENT_ABAPI)) ||    // or invalid data
#else
		|STARTUP_INBOX|STARTUP_CLIENT_MAPI|STARTUP_COMPOSE|STARTUP_FOLDER|STARTUP_FOLDERS|STARTUP_CALENDAR)) ||    // or invalid data
#endif /* MOZ_MAIL_NEWS */
        m_bKioskMode ))  {                                                  // or kiosk mode - start browser

 #ifdef EDITOR
        //  The Editor is started INSTEAD of the Browser,
        if ( bIsGold && (iStartupMode & STARTUP_EDITOR) ) 
		{
            FE_LoadUrl(pszCmdLine, LOAD_URL_COMPOSER);
            return;
        } 
		else 
#endif /* EDITOR */
		{
            CFrameWnd *pFrameWnd = FEU_GetLastActiveFrame(MWContextBrowser); 
   			if (!theApp.m_ViewTmplate)
   			{
   				AfxMessageBox(IDS_NO_RUN_WHILE_PROFILING);
   				return;
   			}
			else if ( pszCmdLine != NULL && strlen(pszCmdLine) > 0)
            {
                CString strUrl;
		        WFE_ConvertFile2Url(strUrl, pszCmdLine);
 
		        //  Open it in a new window.
		        //  Use the first window as the provider of the context.
		        URL_Struct *pUrl = NET_CreateURLStruct(strUrl, NET_DONT_RELOAD);
		        if(m_pFrameList && m_pFrameList->GetMainContext())
		        {
			        MWContext *pContext = m_pFrameList->GetMainContext()->GetContext();
			        CFE_CreateNewDocWindow(pContext, pUrl);
		        }
		        else
		        {
			        CFE_CreateNewDocWindow(NULL, pUrl);
		        }
                return;
            }
            else
				theApp.m_ViewTmplate->OpenDocumentFile(NULL);
		}


        CMainFrame *pFrame;
        CGenericFrame *pGenFrame;
		if (!theApp.m_pFrameList) 
			return;
        for(pGenFrame = theApp.m_pFrameList; pGenFrame->m_pNext; pGenFrame = pGenFrame->m_pNext) {
        	/* No Body */;
        }

        pFrame = (CMainFrame *) pGenFrame;

        //	The new frame will be the last one in the application's frame list.

        pFrame->OnLoadHomePage();
		
		// possibly clear URL bar focus here. - JRE			
    }

}

CString CNetscapeApp::BuildDDEFile()
{
	char szbuffer[256];
	char szName[256];
	CString strPrefix;

	strPrefix.LoadString(IDS_STUB_PREFIX);


#ifdef XP_WIN32
	int nSize = GetTempPath(511, szbuffer);
	if (nSize == 0)
	{
		AfxMessageBox(IDS_NO_TEMP_DIRECTORY);
		return (CString)" ";
	}
	szbuffer[nSize+1] = '\0'; //terminate the string
	GetTempFileName(szbuffer,strPrefix, 0,szName);
#else
	GetTempFileName(0,strPrefix, 0,szName);
#endif
	return (CString)szName;

}


void CNetscapeApp::BuildStubFile()
{
	char szbuffer[256];
	char szName[256];
	CString strPrefix;

	strPrefix.LoadString(IDS_STUB_PREFIX);

	if (!m_strStubFile.IsEmpty()) 
		return;

#ifdef XP_WIN32
	int nSize = GetTempPath(511, szbuffer);
	if (nSize == 0)
	{
		AfxMessageBox(IDS_NO_TEMP_DIRECTORY);
		return;
	}
	szbuffer[nSize+1] = '\0'; //terminate the string
	GetTempFileName(szbuffer,strPrefix, 0,szName);
#else
	GetTempFileName(0,strPrefix, 0,szName);
#endif
	m_strStubFile = szName;

}

#ifdef MOZ_MAIL_NEWS
BOOL CNetscapeApp::LaunchComposeAndAttach(CStringList &rlstrArgumentList)
{

	void OpenDraftExit (URL_Struct *url_struct, int/*status*/,MWContext *pContext);

	CFileStatus rStatus;
	MSG_AttachmentData *pAttachList = NULL;
	POSITION rPos = rlstrArgumentList.GetHeadPosition();
	char *pszMessage = NULL;     
	char *pszAttachments = NULL;
    
	BuildStubFile();

	if (rlstrArgumentList.GetCount() > 1)
	{

		pszMessage     =  (char*)(const char*)rlstrArgumentList.GetNext(rPos);
		pszAttachments =  (char*)(const char*)rlstrArgumentList.GetNext(rPos);
		
		if ( (*pszMessage == '@') || (*pszMessage == '-') )
			return FALSE;

		//Make sure the files exist.
		if(!CFile::GetStatus(pszMessage,rStatus))
			return FALSE;

		if(!CFile::GetStatus(pszAttachments+1,rStatus))
			return FALSE;
	}
	else
	{   //there was no message file given so we have to fake one to get compose to start
		CStdioFile file;
		if(!CFile::GetStatus(m_strStubFile,rStatus))
		{
			if (!file.Open(m_strStubFile, CFile::modeCreate | CFile::modeWrite))
			{
				return 0;
			}
		}
		else
		{
			if (!file.Open(m_strStubFile, CFile::modeWrite))
			{
				return 0;
			}
		}
		pszAttachments =  (char*)(const char*)rlstrArgumentList.GetNext(rPos);
	}
	
	pAttachList = BuildAttachmentListFromFile(pszAttachments);


	if (pAttachList)
	{
		CString strUrl;
		if (pszMessage)
			WFE_ConvertFile2Url(strUrl, pszMessage);
		else 
			WFE_ConvertFile2Url(strUrl, m_strStubFile);

		URL_Struct *pURL = NET_CreateURLStruct(strUrl, NET_DONT_RELOAD);
		StrAllocCopy(pURL->content_type,"message/rfc822"); 
		pURL->fe_data = (void*)pAttachList;
		NET_GetURL (pURL, FO_CMDLINE_ATTACHMENTS, m_pBmContext,
				   OpenDraftExit);
		
		return TRUE;
	}
	else
		return FALSE;
}


MSG_AttachmentData * CNetscapeApp::BuildAttachmentListFromFile(char *pszAttachFile)
{
	//handle a single attachment here
	if (*pszAttachFile == '-') 
	{
		pszAttachFile += 1;
		CFileStatus rStatus;
		if (CFile::GetStatus(pszAttachFile, rStatus))
		{
			MSG_AttachmentData * pAttach = 
				(MSG_AttachmentData *)XP_CALLOC(2,sizeof(MSG_AttachmentData));
			XP_MEMSET (pAttach, 0, (2) * sizeof (MSG_AttachmentData));
			CString cs;
			WFE_ConvertFile2Url(cs,(const char *)pszAttachFile);
			pAttach->url = XP_STRDUP(cs);
			ASSERT(pAttach->url);
			return pAttach;
		}
	}//handle multiple attachments contained in a list file
	else if(*pszAttachFile == '@')
	{
		//we have to read from the file of attachments
		CStdioFile file;
		MSG_AttachmentData * pAttachList = NULL;
		MSG_AttachmentData *pAttach = NULL;
		pszAttachFile += 1;

		if (!file.Open(pszAttachFile,CFile::modeRead | CFile::typeText))
			return NULL;  //The list file did not exist!!!!
		
		CString strAttach;
		CFileStatus rStatus;
		CStringList rlstrFileList;
		
#ifdef XP_WIN16
		char buf[256];
		while(file.ReadString(buf,255))
		{
			if(CFile::GetStatus(buf,rStatus))
				rlstrFileList.AddTail((CString)buf);
			else
				if ( IDNO == AfxMessageBox(IDS_CMDLINE_ERROR6, MB_YESNO |MB_ICONEXCLAMATION))
					return NULL;
			memset(buf,'\0',256);
		}//build an intermediate list which will be used to create a MSG_AttachmentData list 
#else		
		while(file.ReadString(strAttach))
		{
			if(CFile::GetStatus(strAttach,rStatus))
				rlstrFileList.AddTail(strAttach);
			else
				if ( IDNO == AfxMessageBox(IDS_CMDLINE_ERROR6, MB_YESNO |MB_ICONEXCLAMATION))
					return NULL;
		}//build an intermediate list which will be used to create a MSG_AttachmentData list 
#endif
		int nNumFiles = rlstrFileList.GetCount();
		
		if (nNumFiles > 0)
		{
		    //make sure the attachment is a real file.
			CString cs;
			int count = 0;
			pAttachList = (MSG_AttachmentData *)XP_CALLOC(nNumFiles +1,sizeof(MSG_AttachmentData));
			XP_MEMSET (pAttachList, 0, (nNumFiles +1) * sizeof (MSG_AttachmentData));

			if (!pAttachList)
				return NULL;
	
			POSITION pos1, pos2;
			for( pos1 = rlstrFileList.GetHeadPosition(); ( pos2 = pos1 ) != NULL; )
			{
				strAttach = rlstrFileList.GetNext( pos1 );

				WFE_ConvertFile2Url(cs,(const char *)strAttach);
				pAttachList[count++].url = XP_STRDUP(cs);
			}
			return pAttachList;
		}
	}
	return NULL;

}
#endif /* MOZ_MAIL_NEWS */

////////////////////////////////////////////////////////////////////////////
//Function:		CNetscapeApp::SetStartupMode
//
//Parameters:	iStartupMode. 
//
//Purpose:		Initialize iStartupMode with the appropriate component launch
//				value.
/////////////////////////////////////////////////////////////////////////////				
void CNetscapeApp::SetStartupMode(int32 *iStartupMode)
{
	// set launch type and clear global flag.
	if (m_bAccountSetup)		{                  // MUST be first to ensure only in browser
		*iStartupMode=	STARTUP_ACCOUNT_SETUP;
		m_bAccountSetup= FALSE;

#ifdef EDITOR
	}else if ( m_bCmdEdit || m_bCreateEdit)	{
		*iStartupMode	=	STARTUP_EDITOR;
		m_bCmdEdit=m_bCreateEdit = 0;
#endif /* EDITOR */	

#ifdef MOZ_MAIL_NEWS
	}else 	if (m_bCreateInbox)			{
				*iStartupMode=	STARTUP_INBOX;
				m_bCreateInbox= 0;

  // rhp - for MAPI startup
  }else 	if (m_bCreateInboxMAPI)			{
				*iStartupMode=	STARTUP_CLIENT_MAPI;
				m_bCreateInboxMAPI= 0;
  // rhp - for MAPI startup

	}else 	if (m_bCreateFolder)		{
				*iStartupMode=	STARTUP_FOLDER;
				m_bCreateFolder= 0;

	}else 	if (m_bCreateFolders)		{
				*iStartupMode=	STARTUP_FOLDERS;
				m_bCreateFolders= 0;

	}else	if (m_bCreateCompose)		{
				*iStartupMode=	STARTUP_COMPOSE;
				m_bCreateCompose= 0;

	}else	if (m_bCreateAddress)		{
				*iStartupMode=	STARTUP_ADDRESS;
				m_bCreateAddress= 0;

#endif /* MOZ_MAIL_NEWS */
	}else	if (m_bCreateLDIF_IMPORT)	{
				*iStartupMode=	STARTUP_IMPORT;
				m_bCreateLDIF_IMPORT= 0;

	}else	if (m_bCreateLDIF_EXPORT)	{
				*iStartupMode=	STARTUP_EXPORT;
				m_bCreateLDIF_EXPORT= 0;

#ifdef MOZ_MAIL_NEWS
	} else	if (m_bCreateMail)			{ //same as folder
				*iStartupMode=	STARTUP_MAIL;
				m_bCreateMail= 0;

#endif /* MOZ_MAIL_NEWS */
	} else	if (m_bCreateNetcaster)			{
				*iStartupMode=	STARTUP_NETCASTER;
				m_bCreateNetcaster= 0;

#ifdef MOZ_MAIL_NEWS
	} else	if (m_bCreateNews)			{ 
				*iStartupMode=	STARTUP_NEWS;
				m_bCreateNews= 0;

	//over ride prefference settings for this mode
#endif /* MOZ_MAIL_NEWS */
	}else	if (m_bCreateCalendar)			{ 
				*iStartupMode=	STARTUP_CALENDAR;
				m_bCreateCalendar= 0;

	} else	if (m_bCreateBrowser)		{ 
				*iStartupMode=	STARTUP_BROWSER;
				m_bCreateBrowser= 0;
	
	//we don't want to handle preferences in the first instance 
	//if requested to do so by a second instance
	}else	if (m_bCreateNewProfile)		{ 
                *iStartupMode=	(*iStartupMode == 0 ? 0: *iStartupMode);
				m_bCreateNewProfile= 0;

	}else	if (m_bProfileManager)		{ 
                *iStartupMode=	(*iStartupMode == 0 ? 0: *iStartupMode);
				m_bProfileManager= 0;

	}else	if (m_bCreateJavaDebugAgent)		{ 
				*iStartupMode=	STARTUP_JAVA_DEBUG_AGENT;
				m_bCreateJavaDebugAgent = 0;
	
	}else	if (m_bAccountSetupStartupJava) {
				m_bAccountSetupStartupJava = FALSE;
				*iStartupMode=	STARTUP_JAVA;
#ifdef MOZ_MAIL_NEWS
    // rhp - for MAPI startup
    }else 	if (m_bCreateInboxMAPI)			{
				*iStartupMode=	STARTUP_CLIENT_MAPI;
				m_bCreateInboxMAPI= 0;
    // rhp - for Address Book startup
    }else 	if (m_bCreateNABWin)			{
				*iStartupMode=	STARTUP_CLIENT_ABAPI;
				m_bCreateNABWin= 0;
    // rhp - for MAPI startup
#endif /* MOZ_MAIL_NEWS */
	}
	else {
        //  Implied component checking here.
        if(m_CmdLineLoadURL) {
            switch(NET_URL_Type(m_CmdLineLoadURL)) {
                case NETHELP_TYPE_URL: {
                    *iStartupMode = STARTUP_NETHELP;
                    break;
                }
            }
        }
	}

}


BOOL CNetscapeApp::ProcessCommandLineDDE(char *pszDDECommand)
{
	//DDE says we have something to chew on.
            //so let's see what we have!
    char *pszCommand = pszDDECommand;
    char *pszFreeMe = NULL;
    BOOL bDdeException = FALSE;

    if (strcasestr(pszDDECommand,"-DDEEXCEPTION"))
    {
 		char *pDelete = XP_STRDUP(pszDDECommand);
		char *pOpen = pDelete;

        if (strnicmp(pOpen, "[cmdline(\"-DDEEXCEPTION", 25) != 0)   
        {
			pOpen += 23;
		}
		else
        {
            XP_FREE(pDelete);   //Added by CLM - memory leak       
			return FALSE; //bad DDE command syntax
        }
        char *pEnd = strchr(pOpen, '"');

		if (pEnd == NULL)
		{
            XP_FREE(pDelete);   //Added by CLM - memory leak       
			return FALSE;
		}
		// trim the string, and open the file
        *pEnd = '\0';
        
        CFile fin;
        if( !fin.Open( pOpen, CFile::modeRead ) )
        {
            XP_FREE(pDelete);
            remove(pOpen);
            return FALSE;
        }

        char *pszDDEstring = new char[fin.GetLength()+1];
        memset(pszDDEstring,'\0', fin.GetLength()+1);
        bDdeException = TRUE;
        int nBytesRead = 0;

        if (pszDDEstring)
        {
            int nLength = fin.GetLength();
            nBytesRead = fin.Read(pszDDEstring,nLength);
            pszDDEstring[nBytesRead] = NULL;
        }
        fin.Close();
        remove(pOpen);
        XP_FREEIF(pDelete);
        pszFreeMe = pszDDEstring;
        pszCommand = pszDDEstring;
    }



	if (ExistComponentArguments(pszCommand))
	{
		int32 iStartupMode = 0;
		SetStartupMode(&iStartupMode);
		LaunchComponentWindow(iStartupMode,pszCommand);
	}
	else
    {   //We've got something other than an expected switch.
        //It's probably a URL.  Let's check it out and do the right thing.
 		CString strUrl;
		BOOL bTranslate = TRUE;
        char *pDelete = NULL;
       if (!bDdeException)
       {
			//  Copy the string to mess with.
			char *pDelete = XP_STRDUP(pszCommand);
			char *pOpen = pDelete;

            // cmdline format is "[cmdline("%s")]" - no whitespace allowed, one per line
            if (strnicmp(pOpen, "[cmdline(\"", 11) != 0)   
            {
				pOpen += 10;
			}
			else
            {
                XP_FREE(pDelete);   //Added by CLM - memory leak       
				return FALSE; //bad DDE command syntax
            }

            BOOL bJumpQuote = FALSE;
            if (*pOpen == '"') {
                pOpen += 1; //jump over additional quote if it is there.
                bJumpQuote = TRUE;
            }

            //  Search backwards for the end, with some smarts.
            char *pEnd = strrchr(pOpen, ']');
            if(pEnd) {
                *pEnd = '\0';
                pEnd = strrchr(pOpen, ')');
                if(pEnd) {
                    *pEnd = '\0';
                    pEnd = strrchr(pOpen, '\"');
                    if(pEnd && bJumpQuote) {
                        *pEnd = '\0';
                        pEnd = strrchr(pOpen, '\"');
                    }
                }
            }

			if (pEnd == NULL)
			{
                XP_FREE(pDelete);   //Added by CLM - memory leak       
				return FALSE;
			}

			// trim the string, and open the file
            *pEnd = '\0';
            strUrl = pOpen;
        }
        else
        {
		    WFE_ConvertFile2Url(strUrl, pszCommand);
        }

        //  Handle other weird cases, like print here.
        //  Other code is not smart enough.
        if(0 == strnicmp(strUrl, "/print", 6)) {
            theApp.OnDDECommand((char *)(const char *)strUrl);
        }
        else {
            //  some URLs get handled differently.
            switch(NET_URL_Type(strUrl)) {
                case NETHELP_TYPE_URL: {
                    if(theApp.m_pSlaveCX) {
                        theApp.m_pSlaveCX->NormalGetUrl(strUrl);
                    }
                    break;
                }
                default: {
            		//  Open it in a new window.
            		//  Use the first window as the provider of the context.
            		URL_Struct *pUrl = NET_CreateURLStruct(strUrl, NET_DONT_RELOAD);
            		if(m_pFrameList && m_pFrameList->GetMainContext())
            		{
            			if (bTranslate)
            			{
            			MWContext *pContext = m_pFrameList->GetMainContext()->GetContext();
            			CFE_CreateNewDocWindow(pContext, pUrl);
            			}
            			else
            			{
            				CFrameGlue * pFrame =
            				CFrameGlue::GetLastActiveFrame(MWContextBrowser);
            				if (pFrame != NULL && pFrame->GetMainContext() && 
            					pFrame->GetMainContext()->GetContext() &&
            					!pFrame->GetMainContext()->GetContext()->restricted_target)
            				{
            					CAbstractCX * pCX = pFrame->GetMainContext();
            					if (pCX != NULL)
            					{
            						pCX->NormalGetUrl(strUrl);
            					}  /* end if */
            				}  /* end if */
            				else
            				{
            					CFE_CreateNewDocWindow(NULL, pUrl);
            				}  /* end else */
            			}  /* end else */
            		}
            		else
            		{
            			CFE_CreateNewDocWindow(NULL, pUrl);
            		}
                    break;
                }
            }
        }

        if (!bDdeException)
			XP_FREEIF(pDelete);
	}
	//handled!!
    if (bDdeException)
        delete pszFreeMe;

	return TRUE;
}

// CLM:
//   System to find previous instance of program by ourselves
//    or from other applications.
//   Return value from OnNetscapeGoldIsActive() in previous instance 
//      is hwnd of CGenericFrame-derived frame
//
BOOL CALLBACK FindNavigator( HWND hwnd, LPARAM lParam )
{
    if ( hwnd == (HWND)(::SendMessage( hwnd, WM_NG_IS_ACTIVE, 0, 0 )) )
    {
        // Return window found and quit enumerating
        *((HWND*)lParam) = hwnd;
        return FALSE;
    }
    return TRUE;
}

BOOL CMapStringToObNoCase::Lookup(LPCTSTR key, CObject*& rValue) const
{
	CString csKey = key;
	csKey.MakeLower();

	return CMapStringToOb::Lookup(csKey, rValue);
}

BOOL CMapStringToObNoCase::LookupKey(LPCTSTR key, LPCTSTR& rKey) const
{
	ASSERT(0);
	return FALSE;
}


CObject*& CMapStringToObNoCase::operator[](LPCTSTR key)
{
	CString csKey = key;
	csKey.MakeLower();

	return CMapStringToOb::operator[](csKey);
}

void CMapStringToObNoCase::SetAt(LPCTSTR key, CObject* newValue)
{
	CString csKey = key;
	csKey.MakeLower();

	CMapStringToOb::SetAt(csKey, newValue);
}

BOOL CMapStringToObNoCase::RemoveKey(LPCTSTR key)
{
	CString csKey = key;
	csKey.MakeLower();

	return CMapStringToOb::RemoveKey(csKey);
}

inline UINT CMapStringToObNoCase::HashKey(LPCTSTR key) const
{
	CString csKey = key;
	csKey.MakeLower();
	return CMapStringToOb::HashKey(csKey);
}

void WFE_LJ_StartupJava(void)
{
#ifdef OJI
    JVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr) {
        jvmMgr->StartupJVM();
        jvmMgr->Release();
    }
#elif defined(JAVA)
    LJ_StartupJava();
#endif
}

void WFE_LJ_StartDebugger(void)
{
#ifdef OJI
    JVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr) {
        NPIJVMPlugin* jvm = jvmMgr->GetJVM();
        if (jvm) {
            static NS_DEFINE_IID(kISymantecDebuggerIID, NP_ISYMANTECDEBUGGER_IID);
            NPISymantecDebugger* debugger;
            if (jvm->QueryInterface(kISymantecDebuggerIID, (void**)&debugger) == NS_OK) {
                // XXX should we make sure the vm is started first?
                debugger->StartDebugger(NPSymantecDebugPort_SharedMemory);
                debugger->Release();
            }
            jvm->Release();
        }
        jvmMgr->Release();
    }
#elif defined(JAVA)
    LJ_StartDebugger(LJDebugPort_SharedMemory);
#endif
}


void CNetscapeApp::RemoveDDESyntaxAndSwitch(CString strSwitch, char *pszCmdLine)
{
    char *pSavPosition = pszCmdLine;

    if (strstr(pszCmdLine, "cmdline") )
    {
		char *pszFirst = strchr(pszCmdLine, '"');
		if (pszFirst && *pszFirst == '"')
			++pszFirst;
        else
        {   //we might have already ripped off the "cmdline" stuff
            pszFirst = strcasestr(pszCmdLine,strSwitch);
        }
    
		char *pszNext = NULL;
		if (pszFirst)
		{
			pszNext = strrchr(pszFirst,'"');
			if (pszNext)
			{	
				*pszNext = NULL;
				pszCmdLine = pszFirst;
			}
			if (IsRuntimeSwitch(strSwitch,pszCmdLine,TRUE))
			{
				while (isspace(*pszCmdLine))
					++pszCmdLine;
                if (*pszCmdLine == '"') 
                    pszCmdLine++;
                if (pszCmdLine[strlen(pszCmdLine)-1] == '"') 
                    pszCmdLine[strlen(pszCmdLine) -1] = '\0';

                strcpy(pSavPosition,pszCmdLine);
			}
		}
    }
    else
    {
		if (IsRuntimeSwitch(strSwitch,pszCmdLine,TRUE))
		{
			while (isspace(*pszCmdLine))
				++pszCmdLine;
		}
    }

}
