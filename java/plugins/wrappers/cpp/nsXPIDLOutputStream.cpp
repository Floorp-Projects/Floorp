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

#include "nsXPIDLOutputStream.h"

NS_IMPL_ISUPPORTS1(nsXPIDLOutputStream, nsIXPIDLOutputStream)

nsXPIDLOutputStream::nsXPIDLOutputStream()
{
  NS_INIT_ISUPPORTS();
}

nsXPIDLOutputStream::nsXPIDLOutputStream( nsIOutputStream *outputStream )
{
  NS_INIT_ISUPPORTS();
  this->outputStream = outputStream;
  NS_ADDREF( outputStream );
}

nsXPIDLOutputStream::~nsXPIDLOutputStream()
{
  NS_RELEASE( outputStream );
}

NS_IMETHODIMP
nsXPIDLOutputStream::Close()
{
    return outputStream->Close();
}

NS_IMETHODIMP
nsXPIDLOutputStream::Flush()
{
    return outputStream->Flush();
}

NS_IMETHODIMP
nsXPIDLOutputStream::Write( PRUint32 count,
                            PRUint8 *buf,
                            PRUint32 *_retval )
{
    return outputStream->Write( (char *)buf, count, _retval );
}
