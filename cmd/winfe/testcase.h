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

#ifndef _testcase_h_
#define _testcase_h_

#include "testcasemanager.h"

#define TEST1  1
#define TEST2  2


extern void WhiteBox_DeleteMessage1(CTestCaseManager *tc);
extern void TrialTestCase2(CTestCaseManager *tc);

// global variable
class CTestCase;
extern CTestCase QA_TestCase;


class CTestCase {
private:
	CString m_sTestID;
	CString m_sBaseFile;
	CString m_sLogFile;
	CString m_sResultFile;
	CString m_sBeakerFile;
	CString m_sStartDate;
	CString m_sEndDate;
	CString m_sStartTime;
	CString m_sStartTimeString;
	CString m_sEndTime;
	CString m_sEndTimeString;
	CString m_sManualResult;
	CString m_sExpectedResults;
	CString m_sActualResults;
	BOOL    m_bFileCompareResult;
	BOOL	m_bPhaseTwoResult;
	BOOL	m_bOverallResult;
	BOOL    m_bCreateBaseLine;
	CTestCaseManager *m_TCManager;
	
public :	
	CTestCase() { };
	CTestCase(CTestCaseManager *tc);  // create vars
	~CTestCase();  // record end time, log result, delete
	void LogResult();  // Appends run times and results to Overall result file
	void LogBeakerResult();
	CString m_GetLogFile() { return m_sLogFile; }
	BOOL FileCompare();
	void QA_Trace(CString msg);
	void QA_LogError(CString msg);
	void DeleteLogFile() { CFile::Remove(m_sLogFile); }
	void WrapUp();
	void PrintToFile(CString& sFile, CString& str);
	void PhaseTwoIsTrue() { m_bPhaseTwoResult = TRUE; }
	void PhaseTwoIsFalse() { m_bPhaseTwoResult = FALSE; }
	void DoWrapup() { m_TCManager->Wrapup(); }
};

#endif _testcase_h_



