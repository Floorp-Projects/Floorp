#include "stdafx.h"
#include "XMLDOMHelper.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CElementNode::CElementNode(IXMLDOMElementPtr element)
: m_element(element)
{
}

BOOL CElementNode::ChildExists(CString strTag)
{
  IXMLDOMNodeListPtr prefValNodes = m_element->getElementsByTagName(strTag.GetBuffer(0));
  if (prefValNodes)
    return prefValNodes->Getlength() > 0;
  else
    return FALSE;
}

CString CElementNode::GetChildElementValue(CString strTag)
{
  _variant_t value;

  IXMLDOMNodeListPtr prefValNodes = m_element->getElementsByTagName(strTag.GetBuffer(0));
  if (prefValNodes)
  {
    IXMLDOMNodePtr prefValNode = prefValNodes->Getitem(0);  // should be only one
    if (prefValNode)
    { 
      if (prefValNode->childNodes->Getlength() > 0)
        value = prefValNode->childNodes->Getitem(0)->GetnodeValue();  // get the text node child, which contains the text we want.
    }
  }

  return (char*)_bstr_t(value);
}

// Set the text of the child element with the tag strTag. Assumes only one child with this tag.
void CElementNode::SetChildElementValue(CString strTag, CString strValue)
{
  _variant_t value = strValue;

  IXMLDOMNodeListPtr prefValNodes = m_element->getElementsByTagName(strTag.GetBuffer(0));
  if (prefValNodes)
  {
    IXMLDOMNodePtr prefValNode = prefValNodes->Getitem(0);  // should be only one

    if (prefValNode == NULL)
    {
      _variant_t tag = strTag;
      prefValNode = m_element->GetownerDocument()->createElement(_bstr_t(tag));
      m_element->appendChild(prefValNode);
    }

    if (prefValNode)
    { 
      if (prefValNode->childNodes->Getlength() == 0)
      {
        IXMLDOMNodePtr newNode = prefValNode->GetownerDocument()->createTextNode(_bstr_t(value));
        prefValNode->appendChild(newNode);
      }
      else
      {
        prefValNode->childNodes->Getitem(0)->PutnodeValue(value);  // get the text node child, which contains the text we want.
      }
    }
  }

  return;
}

IXMLDOMNodePtr CElementNode::AddNode(CString strTag)
{
  _variant_t tag = strTag;
  IXMLDOMNodePtr newNode = m_element->GetownerDocument()->createElement(_bstr_t(tag));
  m_element->appendChild(newNode);
  return newNode;
}

// Return the value of the specified attribute.
CString CElementNode::GetAttribute(CString strAttributeName)
{
  _variant_t attribValue = m_element->getAttribute(strAttributeName.GetBuffer(0));

  if (attribValue.vt == VT_NULL)
    return "";
  else
    return (char*)_bstr_t(attribValue);
}

void CElementNode::SetAttribute(CString strAttributeName, CString strAttributeValue)
{
  _variant_t attributeName = strAttributeName;
  _variant_t attributeValue = strAttributeValue;

  IXMLDOMAttributePtr attribNode = m_element->getAttributeNode(strAttributeName.GetBuffer(0));
  if (attribNode == NULL)
  {
   
    attribNode = m_element->GetownerDocument()->createAttribute(_bstr_t(attributeName));
    m_element->setAttributeNode(attribNode);
  }

  m_element->setAttribute(_bstr_t(attributeName), _bstr_t(attributeValue));
}

// The name of the node.
CString CElementNode::GetNodeName()
{
  _bstr_t nodeName = m_element->GetnodeName();
  return (char*)nodeName;
}

CString CElementNode::GetXML()
{
  _bstr_t xml = m_element->xml;
  return (char*)xml;
}