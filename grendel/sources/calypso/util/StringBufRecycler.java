/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package calypso.util;

import java.util.*;

class StringBufRecycler extends Recycler
{

 /********************************************************
  * @author   gess   04-16-97 2:02pm
  * @param
  * @return
  * @notes    This subclass of the recycler can actually
              manufacture instances of stringbuf. This is
              behavior that the standard recycler doesn't do,
              and the I expect will change in this class,
              real soon now.
  *********************************************************/
  StringBuf alloc()
  {
    StringBuf result=null;
    if (fCount>0)
    {
      result = (StringBuf)fBuffer[fCount-1];
      fBuffer[--fCount]=null;
      result.setLength(0);
    }
    else result=new StringBuf();
    return result;
  }

  public void panic()
  {
    // Get rid of everything, including ourselves
    super.panic();
    StringBuf.gRecycler = null;
  }
}

