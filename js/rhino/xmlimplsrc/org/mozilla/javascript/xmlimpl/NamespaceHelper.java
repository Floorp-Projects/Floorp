/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.xmlimpl;

import java.util.*;
import org.apache.xmlbeans.XmlCursor;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

class NamespaceHelper
{
    private XMLLibImpl lib;
    private final Map prefixToURI = new HashMap();
    private final Map uriToPrefix = new HashMap();
    // A set of URIs that are used without explicit namespace declaration in scope.
    private final Set undeclared = new HashSet();

    private NamespaceHelper(XMLLibImpl lib)
    {
        this.lib = lib;
        // Insert the default namespace
        prefixToURI.put("", "");
        Set prefixes = new HashSet();
        prefixes.add("");
        uriToPrefix.put("", prefixes);
    }

    /**
     * Declared a new namespace
     *
     * @param prefix
     * @param uri
     * @param declarations
     */
    private void declareNamespace(String prefix, String uri, ObjArray declarations)
    {
        Set prefixes = (Set)uriToPrefix.get(uri);
        if(prefixes == null)
        {
            prefixes = new HashSet();
            uriToPrefix.put(uri, prefixes);
        }

        if(!prefixes.contains(prefix))
        {
            String oldURI = (String)prefixToURI.get(prefix);

            // Add the new mapping
            prefixes.add(prefix);
            prefixToURI.put(prefix, uri);
            if(declarations != null)
                declarations.add(new Namespace(lib, prefix, uri));

            if(oldURI != null)
            {
                // Update the existing mapping
                prefixes = (Set)uriToPrefix.get(oldURI);
                prefixes.remove(prefix);
            }
        }
    }

    /**
     * Updates the internal state of this NamespaceHelper to reflect the
     * existance of the XML token pointed to by the cursor.
     */
    private void processName(XmlCursor cursor, ObjArray declarations)
    {
        javax.xml.namespace.QName qname = cursor.getName();
        String uri = qname.getNamespaceURI();
        Set prefixes = (Set)uriToPrefix.get(uri);
        if(prefixes == null || prefixes.size() == 0)
        {
            undeclared.add(uri);
            if(declarations != null)
                declarations.add(new Namespace(lib, uri));
        }
    }

    /**
     * Updates the internal state of this NamespaceHelper with the
     * namespace information of the element pointed to by the cursor.
     */
    private void update(XmlCursor cursor, ObjArray declarations)
    {
        // Process the Namespace declarations
        cursor.push();
        while(cursor.toNextToken().isAnyAttr())
        {
            if(cursor.isNamespace())
            {
                javax.xml.namespace.QName name = cursor.getName();
                String prefix = name.getLocalPart();
                String uri = name.getNamespaceURI();

                declareNamespace(prefix, uri, declarations);
            }
        }
        cursor.pop();

        // Process the element
        processName(cursor, declarations);

        // Process the attributes
        cursor.push();
        boolean hasNext = cursor.toFirstAttribute();
        while(hasNext)
        {
            processName(cursor, declarations);
            hasNext = cursor.toNextAttribute();
        }
        cursor.pop();
    }

    /**
     * @return Object[] array of Namespace objects in scope at the cursor.
     */
    public static Object[] inScopeNamespaces(XMLLibImpl lib, XmlCursor cursor)
    {
        ObjArray namespaces = new ObjArray();
        NamespaceHelper helper = new NamespaceHelper(lib);

        cursor.push();

        int depth = 0;
        while(cursor.hasPrevToken())
        {
            if(cursor.isContainer())
            {
                cursor.push();
                depth++;
            }

            cursor.toParent();
        }

        for(int i = 0; i < depth; i++)
        {
            cursor.pop();
            helper.update(cursor, null);
        }

        Iterator i = helper.prefixToURI.entrySet().iterator();
        while(i.hasNext())
        {
            Map.Entry entry = (Map.Entry)i.next();
            Namespace ns = new Namespace(lib, (String)entry.getKey(),
                                            (String)entry.getValue());
            namespaces.add(ns);
        }

        i = helper.undeclared.iterator();
        while(i.hasNext())
        {
            Namespace ns = new Namespace(lib, (String)i.next());
            namespaces.add(ns);
        }

        cursor.pop();

        return namespaces.toArray();
    }

    static Namespace getNamespace(XMLLibImpl lib, XmlCursor cursor,
                                  Object[] inScopeNamespaces)
    {
        String uri;
        String prefix;

        if (cursor.isProcinst()) {
            uri = "";
            prefix = "";
        } else {
            javax.xml.namespace.QName qname = cursor.getName();
            uri = qname.getNamespaceURI();
            prefix = qname.getPrefix();
        }

        if (inScopeNamespaces == null)
            return new Namespace(lib, prefix, uri);

        Namespace result = null;
        for (int i = 0; i != inScopeNamespaces.length; ++i) {
            Namespace ns = (Namespace)inScopeNamespaces[i];
            if(ns == null) continue;

            String nsURI = ns.uri();
            if(nsURI.equals(uri))
            {
                if(prefix.equals(ns.prefix()))
                {
                    result = ns;
                    break;
                }

                if(result == null ||
                   (result.prefix() == null &&
                    ns.prefix() != null))
                    result = ns;
            }
        }

        if(result == null)
            result = new Namespace(lib, prefix, uri);

        return result;
    }

    /**
     * @return List of Namespace objects that are declared in the container pointed to by the cursor.
     */
    public static Object[] namespaceDeclarations(XMLLibImpl lib, XmlCursor cursor)
    {
        ObjArray declarations = new ObjArray();
        NamespaceHelper helper = new NamespaceHelper(lib);

        cursor.push();

        int depth = 0;
        while(cursor.hasPrevToken())
        {
            if(cursor.isContainer())
            {
                cursor.push();
                depth++;
            }

            cursor.toParent();
        }

        for(int i = 0; i < depth - 1; i++)
        {
            cursor.pop();
            helper.update(cursor, null);
        }

        if(depth > 0)
        {
            cursor.pop();
            helper.update(cursor, declarations);
        }

        cursor.pop();

        return declarations.toArray();
    }

    /**
     * @return Prefix to URI map of all namespaces in scope at the cursor.
     */
    public static Map getAllNamespaces(XMLLibImpl lib, XmlCursor cursor)
    {
        NamespaceHelper helper = new NamespaceHelper(lib);

        cursor.push();

        int depth = 0;
        while(cursor.hasPrevToken())
        {
            if(cursor.isContainer())
            {
                cursor.push();
                depth++;
            }

            cursor.toParent();
        }

        for(int i = 0; i < depth; i++)
        {
            cursor.pop();
            helper.update(cursor, null);
        }

        cursor.pop();

        return helper.prefixToURI;
    }

    public static void getNamespaces(XmlCursor cursor, Map prefixToURI)
    {
        cursor.push();
        while(cursor.toNextToken().isAnyAttr())
        {
            if(cursor.isNamespace())
            {
                javax.xml.namespace.QName name = cursor.getName();
                String prefix = name.getLocalPart();
                String uri = name.getNamespaceURI();

                prefixToURI.put(prefix, uri);
            }
        }
        cursor.pop();
    }

    public static void removeNamespace(XmlCursor cursor, String prefix)
    {
        cursor.push();
        while(cursor.toNextToken().isAnyAttr())
        {
            if(cursor.isNamespace())
            {
                javax.xml.namespace.QName name = cursor.getName();
                if(name.getLocalPart().equals(prefix))
                {
                    cursor.removeXml();
                    break;
                }
            }
        }
        cursor.pop();
    }
}