// PrefElement.h: interface for the CPrefElement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_)
#define AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_CHOICES 20

class CPrefElement 
{
public:
	CPrefElement();
  CPrefElement(CString strPrefName, CString strPrefDesc, CString strPrefType);
	virtual ~CPrefElement();

  CString GetPrettyNameValueString();
  CString GetSelectedChoiceString();
  BOOL IsChoose();
  BOOL IsLockable() {return m_bLockable; };
  BOOL IsLocked() { return m_bLocked; };
  void SetLocked(BOOL bLocked) { m_bLocked = bLocked; };
  BOOL IsManage() { return m_bManage; };
  void SetManage(BOOL bManage) { m_bManage = bManage; };
  BOOL IsUserAdded() { return m_bUserAdded; };
  void SetUserAdded(BOOL bUserAdded) { m_bUserAdded = bUserAdded; };
  CString GetUIName() { return m_strUIName; };
  CString GetPrefValue() { return m_strPrefValue; };
  void SetPrefValue(CString strValue);
  CString GetPrefName() { return m_strPrefName; };
  CString GetPrefType() { return m_strType; };
  CString GetPrefDescription() { return m_strDescription; };
  CString GetValueFromChoiceString(CString strChoiceString);
  BOOL FindString(CString strFind);
  CString* GetChoiceStringArray() {return m_astrChoiceName; };
  CString XML(int iIndentSize, int iIndentLevel);

  // Called from XML parser.
  void startElement(const char* name, const char** atts);
  void characterData(const char* s, int len);
  void endElement(const char* name);

protected:
  void ReInit();

private:
  // For XML parser handling.
  CString m_strCurrentTag;  // current tag being parsed
  BOOL m_bPrefOpen;         // this object is parsing a pref element

  CString m_strUIName;
  CString m_strPrefValue;
  CString m_strPrefName;
  CString m_strDescription;
  CString m_strType;

  BOOL m_bLocked;
  BOOL m_bLockable;
  BOOL m_bManage;
  BOOL m_bUserAdded;

  CString m_astrChoiceName[MAX_CHOICES+1];  // Array of possible choice names. Last is empty string.
  CString m_astrChoiceVal[MAX_CHOICES+1];   // Matching values for the above names.
  int m_iChoices;

};

#endif // !defined(AFX_PREFELEMENT_H__C93E84F5_104A_4218_972D_E2A384749B59__INCLUDED_)
