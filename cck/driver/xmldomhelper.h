#ifndef XMLDOMHELPER_H
#define XMLDOMHELPER_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#import "msxml.dll"
using namespace MSXML;

// This is a little class for helping to deal with the XML DOM elements.
// It takes care of funny string types and returns everything as CString for
// the class user.
class CElementNode
{
public:
  CElementNode(IXMLDOMElementPtr element);

  CString GetNodeName();
  CString GetChildElementValue(CString strTag);
  void SetChildElementValue(CString strTag, CString strValue);
  CString GetAttribute(CString strAttributeName);
  void SetAttribute(CString strAttributeName, CString strAttributeValue);
  CString GetXML();
  IXMLDOMNodePtr AddNode(CString strTag);
  BOOL ChildExists(CString strTag);

protected:
  IXMLDOMElementPtr m_element;

};

#endif