// PrefElement.cpp: implementation of the CPrefElement class.
//
// This is a class with helper functions for the preferences metadata
//  XML.
//
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PrefsElement.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPrefElement::CPrefElement()
: m_bPrefOpen(FALSE), m_iChoices(0)
{

}

CPrefElement::CPrefElement(CString strPrefName, CString strPrefDesc, CString strPrefType)
: m_bPrefOpen(FALSE), m_iChoices(0), m_strPrefName(strPrefName), m_strUIName(strPrefName), m_strDescription(strPrefDesc), m_strType(strPrefType)
{

}

CPrefElement::~CPrefElement()
{
  
}

// Call when you want to reuse an element object.
void CPrefElement::ReInit()
{
  m_strUIName = "";
  m_strPrefValue = "";
  m_strPrefName = "";
  m_strType = "";
  m_strDescription = "";
  m_bLocked = FALSE;
  m_bLockable = TRUE;
  m_bManage = FALSE;
  m_bUserAdded = FALSE;
  m_iChoices = 0;
}

// For a pref node element, returns a string which is used for the tree ctrl
// label.
CString CPrefElement::GetPrettyNameValueString()
{
  CString strValue = GetPrefValue();

  if (IsChoose())
  {
    strValue = GetSelectedChoiceString();
  }

  CString strRetVal;

  strRetVal.Format("%s  [%s]", m_strUIName, strValue);

  return strRetVal;
}

// Get the ui name of the selected choice for this pref.
CString CPrefElement::GetSelectedChoiceString()
{
  ASSERT( m_iChoices <= MAX_CHOICES);

  for (int i = 0; i < m_iChoices; i++)
  {
    if (m_astrChoiceVal[i].CompareNoCase(m_strPrefValue) == 0)
      return m_astrChoiceName[i];
  }

  return "";

}

// Get the value from the choice string. See also GetSelectedChoiceString().
CString CPrefElement::GetValueFromChoiceString(CString strChoiceString)
{
  ASSERT( m_iChoices <= MAX_CHOICES);

  for (int i = 0; i < m_iChoices; i++)
  {
    if (m_astrChoiceName[i].CompareNoCase(strChoiceString) == 0)
      return m_astrChoiceVal[i];
  }

  return "";
}

// Set the value of the pref. For "choose", pass
// the uiname string, and this fuction will convert
// to the correct value.
void CPrefElement::SetPrefValue(CString strValue)
{
  if (IsChoose())
  {
    m_strPrefValue = GetValueFromChoiceString(strValue);
  }

  else
  {
    m_strPrefValue = strValue;
  }
}


// Called by the parser startElement() handler.
void CPrefElement::startElement(const char* name, const char** atts)
{
  // These are the elements we know how to handle:
  if (stricmp(name, "PREF") == 0)
  {

    m_bPrefOpen = TRUE;
    ReInit();

    // Process the list of attribute/value pairs.
    int i = 0;
    while(atts[i])
    {
      const char* attrName = atts[i++];
      const char* attrVal = atts[i++];

      if (stricmp(attrName, "uiname") == 0)
        m_strUIName = attrVal;
      else if (stricmp(attrName, "prefname") == 0)
        m_strPrefName = attrVal;
      else if (stricmp(attrName, "type") == 0)
        m_strType = attrVal;
      else if (stricmp(attrName, "lockable") == 0)
        m_bLockable = (stricmp(attrVal, "true") == 0);
      else if (stricmp(attrName, "description") == 0)
        m_strDescription = attrVal;
      else if (stricmp(attrName, "useradded") == 0)
        m_bUserAdded = (stricmp(attrVal, "true") == 0);
    }

  }
  else if (stricmp(name, "CHOICES") == 0)
  {
    if (!m_bPrefOpen)
      return;

  }
  else if (stricmp(name, "CHOICE") == 0)
  {
    if (!m_bPrefOpen)
      return;

    // Process the list of attribute/value pairs.
    // There must be exactly 2 attributes: a uiname
    // and a value.
    ASSERT(atts[0] && atts[1] && atts[2] && atts[3] && !atts[4]);

    // If you get this assertion, you have too many <CHOICE> elements inside
    // a <CHOICES> element. Boost MAX_CHOICES in prefselement.h.
    ASSERT( m_iChoices < MAX_CHOICES);

    int i = 0;
    while(atts[i])
    {
      const char* attrName = atts[i++];
      const char* attrVal = atts[i++];

      if (stricmp(attrName, "uiname") == 0)
        m_astrChoiceName[m_iChoices] = attrVal;

      else if (stricmp(attrName, "value") == 0)
        m_astrChoiceVal[m_iChoices] = attrVal;
    }
    m_iChoices++;

  }
  else if (stricmp(name, "VALUE") == 0)
  {
    if (!m_bPrefOpen)
      return;

  }
  else if (stricmp(name, "LOCKED") == 0)
  {
    if (!m_bPrefOpen)
      return;

  }
  else if (stricmp(name, "MANAGE") == 0)
  {
    if (!m_bPrefOpen)
      return;

  }
  else
  {
    return;
  }


  // Save the tag we are handling.
  m_strCurrentTag = name;

}

// Called by the parser. Handle the data for the item we are currently handling.
void CPrefElement::characterData(const char* s, int len)
{
  if (!m_bPrefOpen)
    return;

  if (m_strCurrentTag.CompareNoCase("VALUE") == 0)
  {
    char* p = m_strPrefValue.GetBufferSetLength(len);
    memcpy(p, s, len);
    m_strPrefValue.ReleaseBuffer();
    m_strPrefValue.TrimLeft("\r\n");
    m_strPrefValue.TrimRight("\r\n");

    // Done with the data for this tag.
    m_strCurrentTag = "";

  }
  else if (m_strCurrentTag.CompareNoCase("LOCKED") == 0)
  {
    CString tmp;
    char* p = tmp.GetBufferSetLength(len);
    memcpy(p, s, len);
    tmp.ReleaseBuffer();
    tmp.TrimLeft("\r\n");
    tmp.TrimRight("\r\n");
   
    m_bLocked = (tmp.CompareNoCase("true") == 0);

    // Done with the data for this tag.
    m_strCurrentTag = "";
  }
  else if (m_strCurrentTag.CompareNoCase("MANAGE") == 0)
  {
    CString tmp;
    char* p = tmp.GetBufferSetLength(len);
    memcpy(p, s, len);
    tmp.ReleaseBuffer();
    tmp.TrimLeft("\r\n");
    tmp.TrimRight("\r\n");
   
    m_bManage = (tmp.CompareNoCase("true") == 0);

    // Done with the data for this tag.
    m_strCurrentTag = "";
  }

}

// Called by the parser. Handle the end tag.
void CPrefElement::endElement(const char* name)
{
  if (stricmp(name, "PREF") == 0)
  {
    m_bPrefOpen = FALSE;
  }
  else if (stricmp(name, "CHOICES") == 0)
  {
    m_astrChoiceName[m_iChoices] = "";
    m_astrChoiceVal[m_iChoices] = "";
  }

  m_strCurrentTag = "";
}

// Return TRUE if the search string exists in any
// of the prefs fields.
BOOL CPrefElement::FindString(CString strFind)
{
  strFind.MakeUpper();

  CString str = GetUIName();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  str = GetPrefValue();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  str = GetPrefName();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  str = GetPrefType();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  str = GetPrefDescription();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  return FALSE;
}

BOOL CPrefElement::IsChoose()
{ 
  return (m_iChoices > 0); 
}

// Make an XML element string for this pref.
CString CPrefElement::XML(int iIndentSize, int iIndentLevel)
{
  int iIndentSpaces = iIndentSize * iIndentLevel;

  CString strRet;
  strRet.Format("%*s<PREF uiname=\"%s\" prefname=\"%s\" type=\"%s\" lockable=\"%s\" description=\"%s\" useradded=\"%s\">\n", 
    iIndentSpaces, " ", GetUIName(), GetPrefName(), GetPrefType(),
    IsLockable()?"true":"false", GetPrefDescription(), IsUserAdded()?"true":"false");

  CString strTmp;
  if (IsChoose())
  {
    strTmp.Format("%*s<CHOICES>\n", iIndentSpaces + iIndentSize, " ");
    strRet += strTmp;

    for (int i = 0; i < m_iChoices; i++)
    {
      strTmp.Format("%*s<CHOICE uiname=\"%s\" value=\"%s\"/>\n", iIndentSpaces + iIndentSize + iIndentSize, " ", m_astrChoiceName[i], m_astrChoiceVal[i]);
      strRet += strTmp;
    }

    strTmp.Format("%*s</CHOICES>\n", iIndentSpaces + iIndentSize, " ");
    strRet += strTmp;
  }

  strTmp.Format("%*s<VALUE>%s</VALUE>\n", iIndentSpaces + iIndentSize, " ", GetPrefValue());
  strRet += strTmp;

  strTmp.Format("%*s<LOCKED>%s</LOCKED>\n", iIndentSpaces + iIndentSize, " ", IsLocked()?"true":"false");
  strRet += strTmp;

  strTmp.Format("%*s<MANAGE>%s</MANAGE>\n", iIndentSpaces + iIndentSize, " ", IsManage()?"true":"false");
  strRet += strTmp;

  CString strCloseTag;
  strCloseTag.Format("%*s</PREF>\n", iIndentSpaces, " ");

  strRet += strCloseTag;

  return strRet;
}