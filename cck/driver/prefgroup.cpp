// PrefGroup.cpp: implementation of the CPrefGroup class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PrefGroup.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPrefGroup::CPrefGroup(IXMLDOMElementPtr element)
: CElementNode(element)
{

}

CPrefGroup::~CPrefGroup()
{

}

CString CPrefGroup::GetUIName()
{
  return GetAttribute("uiname");
}

