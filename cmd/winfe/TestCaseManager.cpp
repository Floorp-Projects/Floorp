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
// TestCaseManager.cpp : implementation file
//
#include "stdafx.h"
#include "resource.h"
//#include "TestCaseManager.h"
#include "testcase.h"
#include "qa.h"

#include <stdlib.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// To be used by Par's code
CTestCase QA_TestCase;

IMPLEMENT_SERIAL(CTCData, CObject, 0)
IMPLEMENT_SERIAL(CTestCaseManager, CObject, 0)




CTCData::CTCData()
{
	m_strName = "";
	m_strDesc = "";
	m_nID = 0;
}

void CTCData::Serialize(CArchive& ar)
{
	
	if (ar.IsStoring())
	{
		ar << m_strName;
		ar << m_strDesc;
		ar << (WORD)m_nID;
	}
	else
	{
		ar >> m_strName;
		ar >> m_strDesc;
		ar >> (WORD&)m_nID;
	}
}



CTestCaseManager::CTestCaseManager()
{

	m_iNumTestCases = 0; 
	m_iNumTestCasesToRun = 0; 
	m_sDataFile = CString("");
	m_sDataPath = CString("");
	m_sLogFile = CString("");
}

CTestCaseManager::~CTestCaseManager()
{
	DeleteData();
}

void CTestCaseManager::Serialize(CArchive& ar)
{
	CTCData* pTestCase;
	int nI;

	CObject::Serialize(ar);

	if ( ar.IsStoring() )
	{
		ar << (WORD)m_TCData.GetSize();
		for (nI = 0;nI < m_TCData.GetSize();nI++)
		{
			pTestCase = (CTCData*)m_TCData[nI];
			ar << pTestCase;
		}
	}
	else
	{
		WORD wSize;
		ar >> wSize;
		m_TCData.SetSize(wSize);
		for (nI = 0;nI < m_TCData.GetSize();nI++)
		{
			ar >> pTestCase;
			m_TCData.SetAt(nI,pTestCase);
		}
	}

}

void CTestCaseManager::AddTestCase(CTCData* pTestCase)
{
	CTCData *pTCData;	
	int nI;
	// put it in its place (acsending sorted order)

	for (nI = 0;nI < m_TCData.GetSize();nI++)
	{
		pTCData = (CTCData*)m_TCData[nI];
		if (pTCData->m_strName.Compare(pTestCase->m_strName) > 0)
			break;
	}
	m_TCData.InsertAt(nI,pTestCase);

}

void CTestCaseManager::InitRun()
{
	
	CFile fFile;

	
	m_iNumPassed = 0;
	m_iNumFailed = 0;
	m_sLogFile = (CString)(m_sDataPath + "results.all");

	// Delete the .all file
	if (FileExists(m_sLogFile))
		fFile.Remove(m_sLogFile);	

	fFile.Open(m_sLogFile,CFile::modeCreate | CFile::shareDenyNone);
	fFile.Close();
	


}

// appends [str] to the Log File
void CTestCaseManager::PrintToFile(CString& str)
{
	CStdioFile fFile;

	if (fFile.Open(m_sLogFile,CFile::modeWrite | CFile::shareDenyNone))
	{  // write to Testcase Log File
		fFile.Seek(0, CFile::end);
		fFile.WriteString(str + "\n");
		fFile.Flush();
		fFile.Close();
	}
}
void CTestCaseManager::RemoveTestCases(LPINT m_TestCasesToRemove, int nNum)
{
	
	CTCData* pTCData;
	
		
	for (int nI = nNum-1 ; nI >= 0 ; nI--)
	{
		pTCData = (CTCData*)m_TCData[m_TestCasesToRemove[nI]];
		delete pTCData;
		m_TCData.RemoveAt(m_TestCasesToRemove[nI]);
	}
}

void CTestCaseManager::DeleteData()
{
	CTCData* pTCData;
	int nI;

	for (nI = 0;nI < m_TCData.GetSize();nI++)
	{
		pTCData = (CTCData*)m_TCData[nI];
		delete pTCData;
	}
	m_TCData.RemoveAll();
}

void CTestCaseManager::SaveData() 
{
	CFile file;
	

	if ( m_sDataFile == "" )
	{
		CFileDialog dlg(FALSE, NULL, NULL, NULL, "Testcase Data Files (*.dat)|*.dat|All Files (*.*)|*.*||");
		if ( dlg.DoModal() == IDOK )
		{
			m_sDataFile = dlg.GetPathName();
		
			if ( !FileExists(m_sDataFile) )
				return;
		}
	}
	if ( file.Open(m_sDataFile,CFile::modeCreate | CFile::modeWrite | CFile::shareDenyNone) )
	{
			CArchive saveArchive(&file, CArchive::store | CArchive::bNoFlushOnDelete);
			Serialize(saveArchive);
			saveArchive.Close();
			file.Close();
	}
}

void CTestCaseManager::LoadData() 
{
	CFileDialog dlg(TRUE, NULL, NULL, NULL, "Testcase Data Files (*.dat)|*.dat|All Files (*.*)|*.*||");
	CString sFile;
	
	CFile file;

	if ( dlg.DoModal() == IDOK )
	{
		
		m_sDataFile = dlg.GetPathName();

		
		if ( !FileExists(m_sDataFile) )
			return;

		// Save the path
		int iSlantPos = m_sDataFile.ReverseFind('\\');
		m_sDataPath = m_sDataFile.Left(iSlantPos+1);  // get the slant too!

		if ( file.Open(m_sDataFile,CFile::modeRead | CFile::shareDenyNone) )
		{
			DeleteData(); // delete data first

			CArchive loadArchive(&file,CArchive::load | CArchive::bNoFlushOnDelete);
			Serialize(loadArchive);
			loadArchive.Close();
			file.Close();
		}
	}
}


void CTestCaseManager::ExecuteTestCases(LPINT m_TestCasesToRun)
{
	int index;
	CTCData* pTCData;
		
	CTestCaseManager *tc;
	tc = this;

	InitRun();
	while (*m_TestCasesToRun != -1)
	{
		
		index = *m_TestCasesToRun;
		pTCData = (CTCData*)m_TCData[index];
		m_strName = pTCData->m_strName;

		// Examine the following very closely - not a cast but a constructor
		QA_TestCase = (CTestCase)(this); // formerly tc
		switch (pTCData->m_nID)
		{
			case TEST1 : WhiteBox_DeleteMessage1(this);  // formerly tc
						 break;
			case TEST2 : TrialTestCase2(tc);
						 break;
		}
		while (QATestCaseDone == FALSE) {
			FEU_StayingAlive();
		}
		Wrapup();  
		m_TestCasesToRun++;

	}
	DeleteData();
	
}

void CTestCaseManager::Wrapup()
{
	CFile fFile;

	// create the runflag file to talk to QAP
	fFile.Open(m_sDataPath + "runflag.txt",CFile::modeCreate | CFile::shareDenyNone);
	fFile.Close();

	QA_TestCase.WrapUp();	
}

BOOL CTestCaseManager::FileExists(CString& strPath)
{
   CFileStatus status;
   BOOL bFileExists = TRUE;

   if ( !CFile::GetStatus(strPath,status) ) // static member
     bFileExists = FALSE;

   return bFileExists;
}
