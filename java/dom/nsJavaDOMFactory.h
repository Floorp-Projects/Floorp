/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

#ifndef __nsJavaDOMFactory_h__
#define __nsJavaDOMFactory_h__

#include "nsIFactory.h"

#define NS_JAVADOMFACTORY_IID \
  {0x6a07c434, 0x0e58, 0x11d3, \
    {0x86, 0xea, 0x00, 0x04, 0xac, 0x56, 0xc4, 0xa5}}

class nsJavaDOMFactory : public nsIFactory {
  NS_DECL_ISUPPORTS

  public:

  nsJavaDOMFactory();

  NS_IMETHOD CreateInstance(nsISupports* aOuter,
			    const nsIID& iid,
			    void** retval);
  
  NS_IMETHOD LockFactory(PRBool lock);
};

#endif /* __nsJavaDOMFactory_h__ */
