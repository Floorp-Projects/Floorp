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
import java.io.File;
import java.lang.String;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;
import java.util.StringTokenizer;
import java.util.Hashtable;

/*
 *
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */

abstract class FilterEnumeration extends PrefetchEnumeration {
        private Enumeration fDelegate;
        public FilterEnumeration( Enumeration en)
        {
                fDelegate = en;
        }
        abstract protected boolean verify(Object obj);
        protected Object fetch()
        {
                if(fDelegate.hasMoreElements()) {
                        Object  candidcate;
                        for( candidcate = fDelegate.nextElement();
                                fDelegate.hasMoreElements();
                                        candidcate = fDelegate.nextElement())
                        {
                                if(this.verify(candidcate))
                                        return candidcate;
                        }
                }
                return null;
        }
}

class ZipFileEnumeration implements Enumeration {
    class StringStartEndFilterEnumeration extends FilterEnumeration {
        private String fStart;
        private String fEnd;
        public StringStartEndFilterEnumeration(
                Enumeration en,
                String start,
                String end
        )
        {
                super(en);
                fStart = start;
                fEnd = end;
        }
        protected boolean verify(Object obj)
        {
                String str = (String)((ZipEntry) obj).getName();
                return str.startsWith(fStart) && str.endsWith(fEnd);
        }
    }
        private Enumeration fDelegate;
        private String fPrefix;
        private String fPostfix;

        public ZipFileEnumeration (
                File zip ,String pkg, String prefix, String postfix)
        {
                fPrefix = pkg.replace('.', '/') + '/' + prefix;
                fPostfix = postfix;
                try {
                        ZipFile zf = new ZipFile(zip);
                        fDelegate = new
                                StringStartEndFilterEnumeration(
                                        zf.entries(),
                                        fPrefix,
                                        fPostfix
                                );
                }
                catch (Exception e)
                {
                    System.out.println(e);  // For Debug only
                        fDelegate = null;
                }
        }
        public boolean hasMoreElements()
        {
                return ((fDelegate != null) &&
                        fDelegate.hasMoreElements());
        }
        public Object nextElement()
        {
                if(fDelegate == null)
                        return null;
                String current = (String)((ZipEntry) fDelegate.nextElement()).getName();
                return current.substring(
                                fPrefix.length(),
                                (current.length() - fPostfix.length()));
        }
}
class DirectoryEnumeration extends PrefetchEnumeration {
        private String fList[] = null;
        private String fPrefix;
        private String fPostfix;
        private int fIdx = 0;

        public DirectoryEnumeration (
                File dir ,String pkg, String prefix, String postfix)
        {
                fPrefix = prefix;
                fPostfix = postfix;
                File realdir = new File(dir, pkg.replace('.',File.separatorChar));
                if(realdir.exists() && realdir.isDirectory() && realdir.canRead())
                {
                        fList = realdir.list();
                        fIdx = -1;
                }
        }
        protected Object fetch()
        {
            if(fList != null)
        {
                for(fIdx++ ; fIdx < fList.length; fIdx++)
                {
                        if( fList[fIdx].startsWith( fPrefix ) &&
                            fList[fIdx].endsWith( fPostfix ) )
                        {
                                return fList[fIdx].substring(
                                        fPrefix.length(),
                                        (fList[fIdx].length() - fPostfix.length()));
                        }
                }
        }
                return null;
        }
}




public class ClasspathEntryEnumeration extends PrefetchEnumeration  {

        private String fPkg;
        private String fPrefix;
        private String fPostfix;
        private Enumeration fEntryEnumeration;
        private Enumeration fPathEnumeration;
    private Hashtable cache;

/*
 * @author ftang
 * @version $Revision: 1.1 $
 * @see
 *
 */
        public ClasspathEntryEnumeration( String pkg, String prefix, String postfix)
        {
                fPrefix = prefix;
                fPostfix = postfix;
                fPkg = pkg;
                fFetch = null;
                cache = new Hashtable(20);

                fPathEnumeration = new StringTokenizer(
                                        System.getProperty("java.class.path"),
                                        File.pathSeparator);
                fEntryEnumeration = null;
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

                if((fEntryEnumeration != null) &&
                        fEntryEnumeration.hasMoreElements())
            {
                        Object candidcate = fEntryEnumeration.nextElement();

                        if(cache.get(candidcate) != null)
                            return fetch(); // have been report once, try next
                cache.put(candidcate, candidcate);
                return candidcate;

                }
                if(fPathEnumeration.hasMoreElements())
                {
                        String newPathEntry = (String) fPathEnumeration.nextElement();
                        File newPathBase = new File(newPathEntry);
                        if(newPathBase.exists() && newPathBase.canRead())
                        {
                                if(newPathBase.isDirectory())
                                {
                                        fEntryEnumeration =
                                                new DirectoryEnumeration (
                                                        newPathBase, fPkg, fPrefix, fPostfix);
                                }
                                else
                                {
                                        fEntryEnumeration =
                                                new ZipFileEnumeration (
                                                        newPathBase, fPkg, fPrefix, fPostfix);
                                }
                        }
                        return fetch();
                }
                return null;
        }
}

