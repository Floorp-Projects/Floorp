// PrefElement.cpp: implementation of the CPrefElement class.
//
// This is a class with helper functions for the preferences metadata
//  XML.
//
// XML Conventions:
//  Element tags are in CAPS.
//  Attribute names are in lower case.
//  When writing bools, use lower case "true" and "false".
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PrefElement.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPrefElement::CPrefElement(IXMLDOMElementPtr element)
: CElementNode(element)
{

}

CPrefElement::~CPrefElement()
{

}

// For a newly created node, initialize it's settings.
void CPrefElement::Initialize(CString strPrefName, CString strPrefDesc, CString strPrefType)
{
  SetAttribute("uiname", strPrefName);
  SetAttribute("prefname", strPrefName);
  SetAttribute("description", strPrefDesc);
  SetAttribute("type", strPrefType);
  SetAttribute("lockable", "true");
  
  SetLocked(FALSE);
  SetInstallFile("browser.xpi");
  SetPrefFile("bin/defaults/pref/all-ns.js");

  if (strPrefType.CompareNoCase("int") == 0)
    SetPrefValue("0");
  else if (strPrefType.CompareNoCase("bool") == 0)
    SetPrefValue("true");
  else
    SetPrefValue("");
}

// For a pref node element, returns a string which is used for the tree ctrl
// label.
CString CPrefElement::CreateTreeCtrlLabel()
{
  CString strLabel = GetAttribute("uiname");
  CString strValue = GetChildElementValue("VALUE");

  if (IsChoose())
  {
    strValue = GetSelectedChoiceString();
  }

  CString strRetVal;

  strRetVal.Format("%s  [%s]", strLabel, strValue);

  return strRetVal;
}

// Check to see if the pref element is locked.
BOOL CPrefElement::IsLocked()
{
  CString strVal = GetChildElementValue("LOCKED");
  return (strVal.CompareNoCase("true") == 0);
}

// Lock the pref element.
void CPrefElement::SetLocked(BOOL bLocked)
{
  SetChildElementValue("LOCKED", bLocked? "true": "false");
}


// Build a list of choices from the element to pass to the edit dialog.
// Returns an array of CStrings. Caller must delete.
CString* CPrefElement::MakeChoiceStringArray()
{
  CString* choices = NULL;

  IXMLDOMNodeListPtr choiceNodes = m_element->getElementsByTagName("CHOICE");
  if (choiceNodes)
  {
    // Create an array of CString pointers.
    int numChoices = choiceNodes->length;
    choices = new CString[numChoices + 1];
    for (int i = 0; i < numChoices; i++)
    {
      IXMLDOMElementPtr element = choiceNodes->Getitem(i);
      CElementNode elementNode(element); 
      choices[i] = elementNode.GetAttribute("uiname");
    }
    choices[numChoices] = "";
  }
  
  return choices;  
}

// Get the ui name of the selected choice for this pref.
CString CPrefElement::GetSelectedChoiceString()
{
  CString strVal = GetPrefValue();

  IXMLDOMNodeListPtr choiceNodes = m_element->getElementsByTagName("CHOICE");
  if (choiceNodes)
  {
    int numChoices = choiceNodes->length;
    for (int i = 0; i < numChoices; i++)
    {
      IXMLDOMElementPtr element = choiceNodes->Getitem(i);
      CElementNode elementNode(element); 
      if (strVal.CompareNoCase(elementNode.GetAttribute("value")) == 0)
      {
        return elementNode.GetAttribute("uiname");
      }
    }
  }

  return "";
}

CString CPrefElement::GetValueFromChoiceString(CString strChoiceString)
{
  IXMLDOMNodeListPtr choiceNodes = m_element->getElementsByTagName("CHOICE");
  if (choiceNodes)
  {
    int numChoices = choiceNodes->length;
    for (int i = 0; i < numChoices; i++)
    {
      IXMLDOMElementPtr element = choiceNodes->Getitem(i);
      CElementNode elementNode(element); 
      if (strChoiceString.CompareNoCase(elementNode.GetAttribute("uiname")) == 0)
      {
        return elementNode.GetAttribute("value");
      }
    }
  }
  return "";
}

// Get the ui name of the pref.
CString CPrefElement::GetUIName()
{
  return GetAttribute("uiname");
}

// Get the value of the pref.
CString CPrefElement::GetPrefValue()
{
  return GetChildElementValue("VALUE");
}

// Set the value of the pref. For "choose", pass
// the uiname string, and this fuction will convert
// to the correct value.
void CPrefElement::SetPrefValue(CString strValue)
{
  if (IsChoose())
  {
    SetChildElementValue("VALUE", GetValueFromChoiceString(strValue));
  }

  else
  {
    SetChildElementValue("VALUE", strValue);
  }
}



// Get the Mozilla name of the pref.
CString CPrefElement::GetPrefName()
{ 
  return GetAttribute("prefname");
}

// Get the type of the pref. bool, string, int
CString CPrefElement::GetPrefType()
{ 
  return GetAttribute("type");
}

// Get a long description of the pref.
CString CPrefElement::GetPrefDescription()
{
  return GetAttribute("description");
}


// Return name of installation file.
CString CPrefElement::GetInstallFile()
{
  return GetChildElementValue("INSTALLATIONFILE");
}

// Set name of installation file.
void CPrefElement::SetInstallFile(CString strInstallFile)
{
  SetChildElementValue("INSTALLATIONFILE", strInstallFile);
}

// Return name of pref file.
CString CPrefElement::GetPrefFile()
{
  return GetChildElementValue("PREFFILE");
}

// Set name of pref file.
void CPrefElement::SetPrefFile(CString strPrefFile)
{
  SetChildElementValue("PREFFILE", strPrefFile);
}

// Return true if this is a chooser type pref.
BOOL CPrefElement::IsChoose()
{
  return ChildExists("CHOICES");
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

  str = GetInstallFile();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  str = GetPrefFile();
  str.MakeUpper();
  if (str.Find(strFind) >= 0)
    return TRUE;

  return FALSE;
}

void CPrefElement::SetSelected(BOOL bSelected)
{
  SetChildElementValue("SELECTED", bSelected?"true":"false");
}


BOOL CPrefElement::IsSelected()
{
  CString strVal = GetChildElementValue("SELECTED");
  return (strVal.CompareNoCase("true") == 0);
}
