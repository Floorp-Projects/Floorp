// RegTaskManager.h: interface for the CRegTaskManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGTASKMANAGER_H__516D62F5_00EC_4450_B965_003425CF33E1__INCLUDED_)
#define AFX_REGTASKMANAGER_H__516D62F5_00EC_4450_B965_003425CF33E1__INCLUDED_

#include <vector>

#include "RegTask.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CRegTaskManager  
{
    std::vector<CRegTask *> m_cTasks;
    BOOL m_bNeedReboot;

    CString m_szBinDirPath;

public:
	CRegTaskManager();
	virtual ~CRegTaskManager();

    void SetValue(const TCHAR *szName, const TCHAR *szValue);
	void GetValue(const TCHAR *szName, CString &szValue);
	void SetNeedReboot();
    
    void AddTask(CRegTask *pTask);
    int GetTaskCount() const { return m_cTasks.size(); }
    CRegTask *GetTask(int nIndex) { return m_cTasks[nIndex]; }
};


#endif // !defined(AFX_REGTASKMANAGER_H__516D62F5_00EC_4450_B965_003425CF33E1__INCLUDED_)
