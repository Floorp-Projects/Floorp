/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   Addressable.h -- interface definition for editable views
   Created: Dora Hsu<dora@netscape.com>, 27-Sept-96.
 */



#ifndef _xfe_addressable_h
#define _xfe_addressable_h

class MAddressable //Mix-in class - only can have pure virtual methods
{
public:
  virtual void setHeader(int row, MSG_HEADER_SET header) = 0; 
  virtual int setReceipient(int row, char* pAddressStr) = 0; 
  virtual Boolean removeDataAt(int row) = 0;

  virtual Boolean hasDataAt(int row) = 0;
  virtual void insertNewDataAfter(int row) = 0;

  virtual void	getDataAt(int row, int col, char **pData)= 0;
  virtual int getHeader(int row) = 0;

};

#endif /* _xfe_editable_h */
