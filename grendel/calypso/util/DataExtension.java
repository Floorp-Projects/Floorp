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
 */

import java.util.Properties;
import java.util.Hashtable;
import java.lang.ClassLoader;
import java.net.URLEncoder;

/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
public class DataExtension {
        private Hashtable cache = null;
        private boolean caseInsensitive = false;
        private String prefix;
        private String postfix;
        private String[] urls;
        /*
         *
         * @author ftang
         * @version $Revision: 1.1 $
         * @see
         *
         */
        public DataExtension ( String prefix, String postfix, boolean caseInsensitive, String[] fallbackURLs)
        {
                cache = new Hashtable(20) ;
                this.prefix = prefix;
                this.postfix = postfix;
                this.caseInsensitive = caseInsensitive;
                this.urls = fallbackURLs;

        }

        /*
         *
         * @author ftang
         * @version $Revision: 1.1 $
         * @see
         *
         */
        public String getValue (String key, String Name)
        {
                if(caseInsensitive)
                        key = URLEncoder.encode(key.toLowerCase()).toLowerCase();
                Hashtable group = null;
                group = getGroup(key);
                if(group != null)
                        return (String) group.get(Name);
                else
                        return (String) null;
        }
        /*
         *
         * @author ftang
         * @version $Revision: 1.1 $
         * @see
         *
         */
        synchronized private Hashtable getGroup(String key)
        {
                Object lookup = null;
                lookup = cache.get(key);

                if(lookup != null)
                        return (Hashtable) lookup;

                try {
                        Properties p = new Properties();
                        String name = prefix + key + postfix;
                        p.load( ClassLoader.getSystemResourceAsStream( name ) );
                        cache.put(key, p);
                        return (Hashtable) p;
                } catch(Exception e) {
                        /* TO DO */
                        /* do urls fallback here */
                        return (Hashtable) null;
                }

        }

};
