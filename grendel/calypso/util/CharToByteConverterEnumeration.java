/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Frank Tang <ftang@netscape.com>
 */

package calypso.util;
import java.util.Enumeration;
import sun.io.CharToByteConverter;
/*
 * CharToByteConverterEnumeration return a Enumeration of String
 * which represent available CharToByteConverter in the class path
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */

public class CharToByteConverterEnumeration extends PrefetchEnumeration {
    Enumeration fDelegate;
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
    public CharToByteConverterEnumeration()
    {
        fDelegate = new ClasspathEntryEnumeration("sun.io", "CharToByte", ".class");
    }

/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
    protected boolean verify(Object obj)
    {
        try {
            CharToByteConverter bc = CharToByteConverter.getConverter((String)obj);
            return true;
        } catch (Exception e)
        {
            return false;
        }
    }
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
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
