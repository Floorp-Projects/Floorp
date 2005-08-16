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
 *
 *  Created: Frank Tang <ftang@netscape.com>
 */

package calypso.util;

import java.util.Enumeration;
import sun.io.ByteToCharConverter;
import java.lang.String;

/*
 * ByteToCharConverterEnumeration return a Enumeration of String which
 * represent ByteToCharConverter available in the classpath
 * @author ftang
 * @version $Revision: 1.3 $
 * @see
 *
 */
public class ByteToCharConverterEnumeration extends PrefetchEnumeration {
    Enumeration fDelegate;
  /*
   *
   * @author ftang
   * @version $Revision: 1.3 $
   * @see
   *
   */
    public ByteToCharConverterEnumeration()
    {
        fDelegate = new ClasspathEntryEnumeration("sun.io", "ByteToChar", ".class");
    }

  /*
   *
   * @author ftang
   * @version $Revision: 1.3 $
   * @see
   *
   */
    protected boolean verify(Object obj)
    {
        try {
            ByteToCharConverter bc = ByteToCharConverter.getConverter((String)obj);
            return true;
        } catch (Exception e)
        {
            return false;
        }
    }

  /*
   *
   * @author ftang
   * @version $Revision: 1.3 $
   * @see
   *
   */
    protected Object fetch()
    {
        for(; fDelegate.hasMoreElements(); )
        {
            Object candidcate = fDelegate.nextElement();
            if(verify(candidcate))
            {
                return candidcate;
            }
        }
        return null;
    }
}
