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

class CTCData : public CObject
{
	public:

	CTCData();
	
	DECLARE_SERIAL(CTCData);

	virtual void Serialize(CArchive& ar);

	// testcase Data
	CString m_strName;
	CString m_strDesc;
	int m_nID;
};

class CTestCaseManager : public CObject
{
public:

	CTestCaseManager();
	~CTestCaseManager();

	DECLARE_SERIAL(CTestCaseManager);

	virtual void Serialize(CArchive& ar);

	void ExecuteTestCases(LPINT m_TestCasesToRun);
	void RemoveTestCases(LPINT m_TestCasesToRemove, int nNum);
	void AddTestCase(CTCData *pTestCase);
	void SaveData();
	void LoadData();
	void DeleteData();
	void Wrapup();
	void InitRun();
	void PrintToFile(CString& str);
	BOOL FileExists(CString& strPath);
	CString m_sDataFile;
	CString m_sDataPath;
	CPtrArray m_TCData;
	UINT m_iNumTestCasesToRun;
	UINT m_iNumPassed;
	UINT m_iNumFailed;
	CString GetTestCaseName() { return m_strName; }
private:
	UINT m_iNumTestCases;
	CString m_strName;
	CString m_sLogFile;

}; 







