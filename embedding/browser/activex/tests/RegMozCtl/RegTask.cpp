// RegTask.cpp: implementation of the CRegTask class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "regmozctl.h"
#include "RegTask.h"
#include "RegTaskManager.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRegTask::CRegTask()
{

}

CRegTask::CRegTask(CRegTaskManager *pMgr)
{
    m_pTaskMgr = pMgr;
}

CRegTask::~CRegTask()
{

}

//////////////////////////////////////////////////////////////////////

class CRegTaskRegistry : public CRegTask
{
public:
    CRegTaskRegistry(CRegTaskManager *pMgr) : CRegTask(pMgr)
    {
        m_szDesc = _T("Set BinDirectoryPath registry entry");
    }
    HRESULT DoTask();
};


HRESULT CRegTaskRegistry::DoTask()
{
    CString szBinDirPath;
    m_pTaskMgr->GetValue(c_szValueBinDirPath, szBinDirPath);
    // Create registry key
    CRegKey cKey;
	cKey.Create(HKEY_LOCAL_MACHINE, MOZ_CONTROL_REG_KEY);
	cKey.SetValue(szBinDirPath, MOZ_CONTROL_REG_VALUE_BIN_DIR_PATH);
	cKey.Close();
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

class CRegTaskRegSvr : public CRegTask
{
public:
    CRegTaskRegSvr(CRegTaskManager *pMgr) : CRegTask(pMgr)
    {
        m_szDesc = _T("Register Mozilla Control");
    }
    HRESULT DoTask();
};


HRESULT CRegTaskRegSvr::DoTask()
{
    BOOL bRegister = TRUE;
    CString szBinDirPath;
    m_pTaskMgr->GetValue(c_szValueBinDirPath, szBinDirPath);
	SetCurrentDirectory(szBinDirPath);

	// Now register the mozilla control
	CString szMozCtl = szBinDirPath + CString(_T("\\mozctl.dll"));
	HINSTANCE hMod = LoadLibrary(szMozCtl);
	if (hMod == NULL)
	{
		AfxMessageBox(_T("Can't find mozctl.dll in current directory"));
        return E_FAIL;
	}

    HRESULT hr = E_FAIL;
	FARPROC pfn = GetProcAddress(hMod, bRegister ? _T("DllRegisterServer") : _T("DllUnregisterServer"));
	if (pfn)
	{
		hr = pfn();
	}
	FreeLibrary(hMod);
    return hr;
}

//////////////////////////////////////////////////////////////////////

class CRegTaskPATH : public CRegTask
{
public:
    CRegTaskPATH(CRegTaskManager *pMgr) : CRegTask(pMgr)
    {
        m_szDesc = _T("Set PATH environment variable");
    }
    HRESULT DoTask();
};


HRESULT CRegTaskPATH::DoTask()
{
    CString szBinDirPath;
    m_pTaskMgr->GetValue(c_szValueBinDirPath, szBinDirPath);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT CRegTask::PopulateTasks(CRegTaskManager &cMgr)
{
    cMgr.AddTask(new CRegTaskRegistry(&cMgr));
    cMgr.AddTask(new CRegTaskPATH(&cMgr));
    cMgr.AddTask(new CRegTaskRegSvr(&cMgr));
    return S_OK;
}

