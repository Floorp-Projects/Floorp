/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: nsJavaObjectInfo.h,v 1.1 2001/05/10 18:12:42 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsJavaObjectInfo_h__
#define __nsJavaObjectInfo_h__
#include "nsIJavaObjectInfo.h"
#include "nsIJavaHTMLObject.h"
#include "nsIWFInstanceWrapper.h"

class nsJavaObjectInfo : public nsIJavaObjectInfo
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJAVAOBJECTINFO

  nsJavaObjectInfo(nsISupports* peer);
  virtual ~nsJavaObjectInfo();
  NS_IMETHOD SetType(nsJavaObjectType type);

 private:
  char                           **m_keys, **m_vals;
  PRUint32                       m_argc, m_size, m_step;
  char*                          m_docbase;
  nsISupports*                   m_instance;
  nsJavaObjectType               m_type;
  nsIJavaHTMLObject*             m_owner;
  JavaObjectWrapper*             m_wrapper;
  nsIWFInstanceWrapper*          m_jpiwrapper; 
};

#endif
