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

/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
public abstract class PrefetchEnumeration implements Enumeration {
        protected boolean initialized = false;
        protected Object fFetch = null;
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
        private void init()
        {
            if(! initialized )
            {
                fFetch = fetch();
                initialized = true;
                }
        }
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
        public Object nextElement()
        {
                if(! initialized)
                    init();
                Object current = fFetch;
                fFetch = fetch();
                return current;
        }
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
        public boolean hasMoreElements()
        {
                if(! initialized)
                    init();
                return (fFetch != null);
        }
/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
        abstract protected Object fetch();


}
