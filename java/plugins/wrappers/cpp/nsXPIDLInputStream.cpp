/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Serge Pikalev <sep@sparc.spb.su>
 */

#include "nsXPIDLInputStream.h"
#include "nsIInputStream.h"

NS_IMPL_ISUPPORTS1(nsXPIDLInputStream, nsIXPIDLInputStream)

nsXPIDLInputStream::nsXPIDLInputStream()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLInputStream::nsXPIDLInputStream( nsIInputStream *inputStream )
{
  NS_INIT_ISUPPORTS();
  this->inputStream = inputStream;
  NS_ADDREF( inputStream );
}

nsXPIDLInputStream::~nsXPIDLInputStream()
{
  NS_RELEASE( inputStream );
}

NS_IMETHODIMP
nsXPIDLInputStream::Close()
{
    return inputStream->Close();
}

NS_IMETHODIMP
nsXPIDLInputStream::Available( PRUint32 *_retval )
{
    return inputStream->Available( _retval );
}

NS_IMETHODIMP
nsXPIDLInputStream::Read( PRUint32 count,
                          PRUint8 *buf,
                          PRUint32 *_retval )
{
    return inputStream->Read( (char *)buf, count, _retval );
}
