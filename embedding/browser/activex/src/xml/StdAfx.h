// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__45E5B413_2805_11D3_9425_000000000000__INCLUDED_)
#define AFX_STDAFX_H__45E5B413_2805_11D3_9425_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>

//#include "activexml.h"

extern const CLSID CLSID_MozXMLElement;
extern const CLSID CLSID_MozXMLDocument;
extern const CLSID CLSID_MozXMLElementCollection;
extern const IID LIBID_MozActiveXMLLib;

#include "XMLElement.h"
#include "XMLElementCollection.h"
#include "XMLDocument.h"

extern HRESULT ParseExpat(const char *pBuffer, unsigned long cbBufSize, IXMLDocument *pDocument, IXMLElement **ppElement);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__45E5B413_2805_11D3_9425_000000000000__INCLUDED)
