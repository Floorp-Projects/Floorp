// PrefElement.h: interface for the CPrefElement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_)
#define AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "XMLDOMHelper.h"

class CPrefElement : public CElementNode  
{
public:
	CPrefElement(IXMLDOMElementPtr element);
	virtual ~CPrefElement();

  void Initialize(CString strPrefName, CString strPrefDesc, CString strPrefType);
  CString CreateTreeCtrlLabel();
  BOOL IsLocked();
  void SetLocked(BOOL bLocked);
  CString* MakeChoiceStringArray();
  CString GetSelectedChoiceString();
  CString GetUIName();
  CString GetPrefValue();
  void SetPrefValue(CString strValue);
  CString GetPrefName();
  CString GetPrefType();
  CString GetPrefDescription();
  CString GetValueFromChoiceString(CString strChoiceString);
  CString GetInstallFile();
  void SetInstallFile(CString strInstallFile);
  CString GetPrefFile();
  void SetPrefFile(CString strPrefFile);
  BOOL IsChoose();
  BOOL FindString(CString strFind);
  void SetSelected(BOOL bExpanded);
  BOOL IsSelected();
};

#endif // !defined(AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_)
