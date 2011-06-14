// RegTask.h: interface for the CRegTask class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGTASK_H__28D3BD27_F767_4412_B00B_236E3562B214__INCLUDED_)
#define AFX_REGTASK_H__28D3BD27_F767_4412_B00B_236E3562B214__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CRegTaskManager;

class CRegTask  
{
protected:
    CRegTaskManager *m_pTaskMgr;
    CString m_szDesc;

public:
    static HRESULT PopulateTasks(CRegTaskManager &cMgr);

public:
	CRegTask();
    CRegTask(CRegTaskManager *pMgr);
	virtual ~CRegTask();

    CString GetDescription()
    {
        return m_szDesc;
    }

	virtual HRESULT DoTask() = 0;
};

#endif // !defined(AFX_REGTASK_H__28D3BD27_F767_4412_B00B_236E3562B214__INCLUDED_)
