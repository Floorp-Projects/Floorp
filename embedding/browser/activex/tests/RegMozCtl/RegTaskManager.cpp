// RegTaskManager.cpp: implementation of the CRegTaskManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "regmozctl.h"
#include "RegTaskManager.h"

#include <string.h>
#include <tchar.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRegTaskManager::CRegTaskManager()
{
    m_bNeedReboot = FALSE;
}

CRegTaskManager::~CRegTaskManager()
{

}

void CRegTaskManager::SetNeedReboot()
{
    m_bNeedReboot = TRUE;
}

void CRegTaskManager::AddTask(CRegTask *pTask)
{
    m_cTasks.push_back(pTask);
}


void CRegTaskManager::SetValue(const TCHAR *szName, const TCHAR *szValue)
{
    if (_tcscmp(szName, c_szValueBinDirPath) == 0)
    {
        m_szBinDirPath = szValue;
    }
}

void CRegTaskManager::GetValue(const TCHAR *szName, CString &szValue)
{
    if (_tcscmp(szName, c_szValueBinDirPath) == 0)
    {
        szValue = m_szBinDirPath;
    }
}
