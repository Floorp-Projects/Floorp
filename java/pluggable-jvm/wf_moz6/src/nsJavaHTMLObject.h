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
 * $Id: nsJavaHTMLObject.h,v 1.1 2001/05/10 18:12:42 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsJavaHTMLObject_h
#define __nsJavaHTMLObject_h

#include "nsIJavaHTMLObject.h"
#include "nsIPluggableJVM.h"
#include "nsJavaHTMLObjectFactory.h"
#include "nsIJavaObjectInfo.h"

class nsJavaHTMLObject : public nsIJavaHTMLObject 
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJAVAHTMLOBJECT
  
  nsJavaHTMLObject(nsJavaHTMLObjectFactory* f);
  virtual ~nsJavaHTMLObject();
  
  // generally bad idea, but I don't like to add smth plugin API
  // specific into nsJavaHTMLObject
  // ideally all this info should be obtained from peer
  NS_IMETHOD SetJavaObjectInfo(nsIJavaObjectInfo* info);
  NS_IMETHOD GetJavaObjectInfo(nsIJavaObjectInfo* *info);

 protected:
  nsJavaHTMLObjectFactory* m_factory;
  nsISupports*             m_instance_peer;
  nsIPluggableJVM*         m_jvm;
  nsIJavaObjectInfo*       m_info;
  jint                     m_id;
};

#endif // __nsJavaHTMLObject_h
