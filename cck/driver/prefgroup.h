// PrefGroup.h: interface for the CPrefGroup class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREFGROUP_H__D81DF6AF_427E_4AE4_84E8_86ACFEDBE6D3__INCLUDED_)
#define AFX_PREFGROUP_H__D81DF6AF_427E_4AE4_84E8_86ACFEDBE6D3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "XMLDOMHelper.h"

class CPrefGroup : public CElementNode  
{
public:
	CPrefGroup(IXMLDOMElementPtr element);
	virtual ~CPrefGroup();

  CString GetUIName();
};

#endif // !defined(AFX_PREFGROUP_H__D81DF6AF_427E_4AE4_84E8_86ACFEDBE6D3__INCLUDED_)
