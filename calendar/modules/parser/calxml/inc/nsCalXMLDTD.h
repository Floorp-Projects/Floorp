/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef NS_CALXMLDTD__
#define NS_CALXMLDTD__

#include "nscalexport.h"
#include "CNavDTD.h"
#include "nscalstrings.h"

//*** This enum is used to define the known universe of XIF tags.
//*** The use of this table doesn't preclude of from using non-standard
//*** tags. It simply makes normal tag handling more efficient.
enum eCalXMLTags
{
  eCalXMLTag_unknown=0,   

  eCalXMLTag_attr,
  eCalXMLTag_calendar,
  eCalXMLTag_commandcanvas,
  eCalXMLTag_comment,
  eCalXMLTag_control,
  eCalXMLTag_ctx,
  eCalXMLTag_doctype,
  eCalXMLTag_foldercanvas,
  eCalXMLTag_htmlcanvas,
  eCalXMLTag_leaf,
  eCalXMLTag_mcc,
  eCalXMLTag_multidayviewcanvas,
  eCalXMLTag_object,
  eCalXMLTag_panel,
  eCalXMLTag_rootpanel,
  eCalXMLTag_set,
  eCalXMLTag_tcc,
  eCalXMLTag_timebarscale,
  eCalXMLTag_timebaruserheading,
  eCalXMLTag_todocanvas,
  eCalXMLTag_xml,    
  eCalXMLTag_xpitem,

  eCalXMLTag_userdefined
};

enum eCalXMLAttributes {
  eCalXMLAttr_unknown=0,

  eCalXMLAttr_key,
  eCalXMLAttr_tag,
  eCalXMLAttr_value,

  eCalXMLAttr_userdefined  
};


class nsCalXMLDTD : public CNavDTD {
            
  public:

    NS_DECL_ISUPPORTS

    nsCalXMLDTD();
    virtual ~nsCalXMLDTD();

    virtual PRBool CanParse(nsString& aContentType, PRInt32 aVersion);

    virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType);
    
    NS_IMETHOD HandleToken(CToken* aToken);

    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

    nsresult HandleStartToken(CToken* aToken);
    nsresult HandleEndToken(CToken* aToken);

private:
    NS_IMETHOD_(eCalXMLTags)  TagTypeFromObject(const nsIParserNode& aNode) ;

};

#endif 
