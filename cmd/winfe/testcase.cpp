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
#include "testcase.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// global variable to access Testcase
extern CTestCase QA_TestCase;

// Assigns sTestID, sLogFile and sStartTime
CTestCase::CTestCase(CTestCaseManager *tc)
{
	
	CFile fFile;	
	m_sTestID = tc->GetTestCaseName();
	
	m_TCManager = tc;
	CString sDataPath = tc->m_sDataPath;

	
	m_sBaseFile = (CString)(sDataPath + m_sTestID + ".bas");
	m_sResultFile = (CString)(sDataPath + m_sTestID + ".ran");
	m_sBeakerFile = "c:\\harness.log";  // DOS and Windows only path format
	

	// if BaseLine File doesn't exist then log run to a .bas file instead of a .log file
	if (!m_TCManager->FileExists(m_sBaseFile))
	{
		m_sLogFile = m_sBaseFile;
		m_bCreateBaseLine = TRUE;
	}
	else
	{
		m_sLogFile = (CString)(sDataPath + m_sTestID + ".log");
		// remove previous log file
		if (m_TCManager->FileExists(m_sLogFile))
		{
			CFile fFile;
			fFile.Remove(m_sLogFile);
		}
		m_bCreateBaseLine = FALSE;
	}

		
	// remove .ran file
	if (m_TCManager->FileExists(m_sResultFile))
		fFile.Remove(m_sResultFile);

	// create .ran file
	fFile.Open(m_sResultFile,CFile::modeCreate | CFile::shareDenyNone);
	fFile.Close();

	fFile.Open(m_sLogFile,CFile::modeCreate | CFile::shareDenyNone);
	fFile.Close();

	// create c:\harness.log file 
	fFile.Open(m_sBeakerFile,CFile::modeCreate | CFile::shareDenyNone);
	fFile.Close();
		

	// Log Clock
	char tmpbuf[128];

	 _strdate( tmpbuf );
	m_sStartTimeString = (CString)((CString)"Start Date/Time:  " + tmpbuf + " ");
	m_sStartDate = (CString)(tmpbuf);

	_strtime( tmpbuf );
	m_sStartTimeString += (CString)(tmpbuf);
	m_sStartTime = (CString)(tmpbuf);

	m_sExpectedResults = " ";
	m_sActualResults    = "Some Error";

	// Log a header at the Top of the (.all) file
	CString sHeader = "\n++Testcase : " + m_sTestID;
	m_TCManager->PrintToFile(sHeader);

	PhaseTwoIsFalse();					// added by par
	m_bFileCompareResult = FALSE;		// added by par
}


CTestCase::~CTestCase()
{
	// release memory
	m_sTestID.Empty();
	m_sBaseFile.Empty();
	m_sLogFile.Empty();
	m_sResultFile.Empty();
	m_sStartDate.Empty();
	m_sStartTime.Empty();
	m_sStartTimeString.Empty();
	m_sEndTime.Empty();
	m_sEndTimeString.Empty();
	m_sManualResult.Empty();
}

	
// Compares two log files.  
// returns result.

void CTestCase::LogResult()
{
	CStdioFile fResultFile;
	CString sResult;

	// check if creating baseline or there was an actual compare
	if (!m_bCreateBaseLine)
		sResult = "File Compare Result: " + (m_bFileCompareResult ? "PASS" : "*** FAILED - Check "+ m_sLogFile);
	else
		sResult = "Created BaseLine File - " + m_sBaseFile;

	if (m_bPhaseTwoResult) // TRUE
		sResult = sResult + "\n" + "Phase Two Result: PASS";
	else
		sResult = sResult + "\n" + "Phase Two Result: FAIL";

	if (!m_bCreateBaseLine)
	{
		if (m_bFileCompareResult && m_bPhaseTwoResult) {
			sResult = sResult + "\n" + "Overall Test Result: PASS";
			m_bOverallResult = TRUE;
		} else {
			sResult = sResult + "\n" + "Overall Test Result: FAIL";
			m_bOverallResult = FALSE;
		}
	}
	else
		sResult = sResult + "\n" + "Overall Test Result: N/A";
	
	// Print Result to files (.ran)
	PrintToFile(m_sResultFile,sResult);

	// (.all)
	m_TCManager->PrintToFile(sResult);

		// Print Times to files (.ran)
	CString sTimes = m_sStartTimeString + "\n" + m_sEndTimeString;
	PrintToFile(m_sResultFile,sTimes);

	// (.all)
	m_TCManager->PrintToFile(sTimes);

	
}

// Compares 2 files one line at a time.
BOOL CTestCase::FileCompare()
{
	BOOL bResult=FALSE;
	CStdioFile fBase, fCompare;
	CString sLine_Base, sLine_Comp;

	if (m_TCManager->FileExists(m_sLogFile))
	{
		if (fBase.Open((LPCTSTR)m_sBaseFile,CFile::modeRead) && \
			fCompare.Open((LPCTSTR)m_sLogFile, CFile::modeRead))
		{
			// verify line at a time
			while (fBase.ReadString(sLine_Base) && fCompare.ReadString(sLine_Comp) && bResult)
				bResult = (sLine_Base.Compare(sLine_Comp) == 0 ? 1 : 0);	

			fBase.Close();
			fCompare.Close();
			bResult = TRUE;
		}
	}

	return bResult;
}

void CTestCase::QA_Trace(CString msg)
{
	// write to Log File (.bas if doesn't exist, otherwise .log)
	PrintToFile(m_sLogFile,msg);
}

void CTestCase::QA_LogError(CString msg)
{
	// write to Overall File (.all)
	m_TCManager->PrintToFile(msg);

	// write to Testcase Result File (.ran)
	PrintToFile(m_sResultFile, msg);
}

void CTestCase::WrapUp()
{
	//Log End Time
	char tmpbuf[128];

	 _strdate( tmpbuf );
	m_sEndTimeString = (CString)((CString)"End Date/Time:  " + tmpbuf + " ");
	m_sEndDate = (CString)(tmpbuf);

	_strtime( tmpbuf );
	m_sEndTimeString += (CString)(tmpbuf);
	m_sEndTime = (CString)(tmpbuf);


	if (!m_bCreateBaseLine)
		m_bFileCompareResult = FileCompare();
	
	LogResult();
	LogBeakerResult();  // new
}

void CTestCase::PrintToFile(CString& sFile, CString& str)
{
	CStdioFile fFile;

	if (fFile.Open(sFile, CFile::modeWrite | CFile::shareDenyNone))
	{  
		fFile.Seek(0, CFile::end);
		// write to Testcase Log File
		fFile.WriteString(str + "\n");
		fFile.Flush();
		fFile.Close();
	}
}

void CTestCase::LogBeakerResult()
{
	// Offical Beaker format is
	// (machine), -pass-, Test_case_Name/Beaker, Test_Case_Name, Date, Time Start, Time End, Expected Results, Actual Results

	CStdioFile fResultFile;
	CString sResult;

	sResult = "(local), ";   // standard

	// check if creating baseline or there was an actual compare
	if (!m_bOverallResult)
		sResult = sResult + "*FAIL*, ";
	else
		sResult = sResult + "-pass-, "; 

	CString TestCaseName = m_TCManager->GetTestCaseName();
	sResult = sResult + (LPCTSTR) TestCaseName + "\\Beaker, ";
	sResult = sResult + (LPCTSTR) TestCaseName + ", ";

	// log the date
	char tmpbuf[128];
	 _strdate( tmpbuf );
	CString TodayDate = (CString)(tmpbuf);

	sResult = sResult + m_sStartDate + ", " + m_sStartTime + ", " + m_sEndTime + ", ";

	

	if (!m_bOverallResult) {
		sResult = sResult + m_sExpectedResults + ", ";
		sResult = sResult + m_sActualResults;
	} else {
		// do nothing as no need to post actual results
		sResult = sResult + ", ";
	}

	// Print Result to Beaker file (harness.log)
	PrintToFile(m_sBeakerFile,sResult);
}

