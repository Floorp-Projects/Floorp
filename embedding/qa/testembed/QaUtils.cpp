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
// These are utilities to help with QA tasks.

// Includes routine to post results to a QA log file.

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserView.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "QAUtils.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

storage getSupportObj;

void RvTestResultDlg(nsresult rv, CString pLine,BOOL bClearList)
{
	static CShowTestResults dlgResult ;
	if (!dlgResult)
		dlgResult.Create(IDD_RUNTESTSDLG);

	
	if (bClearList)
	{
		dlgResult.m_ListResults.DeleteAllItems();
		dlgResult.ShowWindow(true);
	}

	if (NS_FAILED(rv))
	   dlgResult.AddItemToList(pLine,false);
	else
	   dlgResult.AddItemToList(pLine,true);

}

void RvTestResult(nsresult rv, const char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file

	CString strLine = pLine;
	char theOutputLine[200];

	if (NS_FAILED(rv))
	   strLine += " failed.";
	else
	   strLine += " passed.";

	strcpy(theOutputLine, strLine);
	QAOutput(theOutputLine, displayMethod);
}

void WriteToOutputFile(const char *pLine) 
{ 
    CStdioFile myFile; 
    CFileException e; 
    CString strFileName = "c:\\temp\\TestOutput.txt"; 

    if(! myFile.Open( strFileName, CStdioFile::modeCreate | CStdioFile::modeWrite
								| CStdioFile::modeNoTruncate, &e ) ) 
    { 
        CString failCause = "Unable to open file. Reason : "; 
        switch (e.m_cause) {
        case CFileException::none:
            failCause += "No error occurred.";
            break;

        case CFileException::generic:
            failCause += "An unspecified error occurred.";
            break;

        case CFileException::fileNotFound:
            failCause += "The file could not be located.";
            break;

        case CFileException::badPath:
            failCause += "All or part of the path is invalid.";
            break;

        case CFileException::tooManyOpenFiles:
            failCause += "The permitted number of open files was exceeded.";
            break;

        case CFileException::accessDenied:
            failCause += "The file could not be accessed.";
            break;

        case CFileException::invalidFile:
            failCause += "There was an attempt to use an invalid file handle.";
            break;

        case CFileException::removeCurrentDir:
            failCause += "The current working directory cannot be removed.";
            break;

        case CFileException::directoryFull:
            failCause += "There are no more directory entries.";
            break;

        case CFileException::badSeek:
            failCause += "There was an error trying to set the file pointer.";
            break;

        case CFileException::hardIO:
            failCause += "There was a hardware error.";
            break;

        case CFileException::sharingViolation:
            failCause += "SHARE.EXE was not loaded, or a shared region was locked.";
            break;

        case CFileException::lockViolation:
            failCause += "There was an attempt to lock a region that was already locked.";
            break;

        case CFileException::diskFull:
            failCause += "The disk is full.";
            break;

        case CFileException::endOfFile:
            failCause += "The end of file was reached.";
            break;

        default:
            failCause += "Some reason not documented in <http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vcmfc98/html/_mfc_cfileexception.3a3a.m_cause.asp>.";
            break;
        }
        AfxMessageBox(failCause);
    } 
    else 
    { 
		myFile.SeekToEnd();
        CString strLine = pLine; 
        strLine += "\r\n"; 

        myFile.WriteString(strLine); 

        myFile.Close(); 
    } 
} 

void QAOutput(const char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file
	// displayMethod 0 = mfc dialog; 1 = output log file; 2 = both
//#if 0
   CString strLine = pLine;

   if (displayMethod == 0)
	   AfxMessageBox(strLine);
   else if (displayMethod == 1)
	   WriteToOutputFile(pLine);
   else 
   {
       WriteToOutputFile(pLine);
	   AfxMessageBox(strLine);
   }
//#endif
}

void FormatAndPrintOutput(const char *theInput, const char *theVar, int outputMode)
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr += theVar;

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, nsCAutoString theVar, int outputMode)
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr += theVar;

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, int theVar, int outputMode) 
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr.AppendInt(theVar);

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, double theVar, int outputMode) 
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr.AppendFloat(theVar);

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, PRUint32 theVar, int outputMode) 
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr.AppendFloat(theVar);

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

// stringMsg is returned in case embeddor wishes to use it in the calling method.
void RequestName(nsIRequest *request, nsCString &stringMsg, int displayMethod)
{
	nsresult rv;

	if (!request) {
		QAOutput("ERROR. nsIRequest object is Null. RequestName test fails.", displayMethod);
		return;
	}
	else
		rv = request->GetName(stringMsg);

	if(NS_SUCCEEDED(rv))																
		FormatAndPrintOutput("nsIRequest: The request name = ", stringMsg.get(), displayMethod);
	else
		QAOutput("nsIRequest: We didn't get the request name.", displayMethod);
}

void WebProgDOMWindowTest(nsIWebProgress *progress, const char *inString,
								  int displayMethod)
{
	nsresult rv;
	nsCString totalStr1, totalStr2;
	nsCOMPtr<nsIDOMWindow> theDOMWindow;

	totalStr1 = inString;
	totalStr1 += ": Didn't get the DOMWindow. Test failed.";

	totalStr2 = inString;
	totalStr2 += ": nsIWebProgress:DOMWindow attribute test";

	if (!progress) {
		QAOutput("ERROR. nsIWebProgress object is Null. WebProgDOMWindowTest fails.", displayMethod);
		return;
	}
	else
		rv = progress->GetDOMWindow(getter_AddRefs(theDOMWindow));
	if (!theDOMWindow)
		QAOutput(totalStr1.get(), displayMethod);
	else
		RvTestResult(rv, totalStr2.get(), displayMethod);
}

void WebProgIsDocLoadingTest(nsIWebProgress *progress, const char *inString,
								  int displayMethod)
{
	nsresult rv;
	PRBool docLoading;
	nsCString totalStr;

	totalStr = inString;
	totalStr += ": nsIWebProgress:IsDocumentLoading attribute test";

	if (!progress) {
		QAOutput("ERROR. nsIWebProgress object is Null. WebProgIsDocLoadingTest fails.", displayMethod);
		return;
	}
	else
	rv = progress->GetIsLoadingDocument(&docLoading);
	RvTestResult(rv, totalStr.get(), displayMethod);
	FormatAndPrintOutput("nsIWebProgress: isDocumentLoading return value = ", docLoading, displayMethod);
}

nsIDOMWindow * GetTheDOMWindow(nsIWebBrowser *webBrowser)
{
	nsCOMPtr<nsIDOMWindow> theDOMWindow;

    webBrowser->GetContentDOMWindow(getter_AddRefs(theDOMWindow));
    if (!theDOMWindow) {
        QAOutput("Didn't get a DOM Window.");
		return nsnull;
	}
	return (theDOMWindow);
}

nsCAutoString GetTheURI(nsIURI *theURI, int displayMethod)
{
	nsresult rv;
	nsCAutoString uriString;

	if (!theURI) {
        QAOutput("nsIURI object is null. return failure.");
		return uriString;  //dep 3/30
	}
	rv = theURI->GetSpec(uriString);
    RvTestResult(rv, "nsIURI::GetSpec() test", displayMethod);
    FormatAndPrintOutput("the uri = ", uriString, displayMethod);

	return uriString;
}

// used for web progress listener in BrowserImplWebPrgrsLstnr.cpp
void onStateChangeString(char *theStateType, char *theDocType, 
						 nsCString stringMsg, PRUint32 status, int displayMode)
{
	nsCString totalMsg;

	totalMsg = "OnStateChange(): ";
	totalMsg += theStateType;
	totalMsg += ", ";
	totalMsg += theDocType;
	totalMsg += ", ";
	totalMsg += stringMsg;
	totalMsg += ", status (hex) = ";
	totalMsg.AppendInt(status, 16);
	QAOutput(totalMsg.get(), displayMode);
}

void SaveObject(nsISupports *theSupports)
{
	getSupportObj.sup = theSupports;
}

/////////////////////////////////////////////////////////////////////////////
// CShowTestResults dialog


CShowTestResults::CShowTestResults(CWnd* pParent /*=NULL*/)
	: CDialog(CShowTestResults::IDD, pParent)
{

	//{{AFX_DATA_INIT(CShowTestResults)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

}


void CShowTestResults::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CShowTestResults)
	DDX_Control(pDX, IDC_LIST1, m_ListResults);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CShowTestResults, CDialog)
	//{{AFX_MSG_MAP(CShowTestResults)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CShowTestResults message handlers

BOOL CShowTestResults::OnInitDialog() 
{
	
	CDialog::OnInitDialog();
	
	m_ListResults.InsertColumn(0,"Test Case",LVCFMT_LEFT,360);
	m_ListResults.InsertColumn(1,"Result",LVCFMT_LEFT,100);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CShowTestResults::AddItemToList(LPCTSTR szTestCaseName, BOOL bResult) 
{
	LV_ITEM lvitem ;
		
	lvitem.mask = LVIF_TEXT;
	lvitem.iItem = m_ListResults.GetItemCount();
	lvitem.iSubItem = 0;
	lvitem.pszText = (LPTSTR)szTestCaseName ;
	//Insert the main item
	m_ListResults.InsertItem(&lvitem);

	if (bResult)
	m_ListResults.SetItemText(m_ListResults.GetItemCount()-1,1,"Passed");
	else
	m_ListResults.SetItemText(m_ListResults.GetItemCount()-1,1,"Failed");

	//Insert the sub item
	//m_ListResults.InsertItem(&lvitem);

}
