/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino serialization code, released
 * Sept. 25, 2001.
 *
 * The Initial Developer of the Original Code is Norris Boyd.
 *
 * Contributor(s):
 * Norris Boyd
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

package org.mozilla.javascript;

import java.util.Hashtable;
import java.util.StringTokenizer;
import java.io.*;

/**
 * Class ScriptableOutputStream is an ObjectOutputStream used
 * to serialize JavaScript objects and functions. Note that
 * compiled functions currently cannot be serialized, only 
 * interpreted functions. The top-level scope containing the 
 * object is not written out, but is instead replaced with 
 * another top-level object when the ScriptableInputStream 
 * reads in this object. Also, object corresponding to names 
 * added to the exclude list are not written out but instead 
 * are looked up during deserialization. This approach avoids
 * the creation of duplicate copies of standard objects 
 * during deserialization.
 *
 * @author Norris Boyd
 */

// API class

public class ScriptableOutputStream extends ObjectOutputStream {

    /**
     * ScriptableOutputStream constructor.
     * Creates a ScriptableOutputStream for use in serializing 
     * JavaScript objects. Calls excludeStandardObjectNames.
     * 
     * @param out the OutputStream to write to.
     * @param scope the scope containing the object.
     */
    public ScriptableOutputStream(OutputStream out, Scriptable scope)
        throws IOException
    {
        super(out);
        this.scope = scope;
        table = new Hashtable(31);
        table.put(scope, "");
        enableReplaceObject(true);
        excludeStandardObjectNames();
    }

    /**
     * Adds a qualified name to the list of object to be excluded from
     * serialization. Names excluded from serialization are looked up
     * in the new scope and replaced upon deserialization.
     * @param name a fully qualified name (of the form "a.b.c", where
     *             "a" must be a property of the top-level object)
     */
    public void addExcludedName(String name) {
        Object obj = lookupQualifiedName(scope, name);
        if (!(obj instanceof Scriptable)) {
            throw new IllegalArgumentException("Object for excluded name " +
                                               name + " not found.");
        }
        table.put(obj, name);
    }

    /**
     * Returns true if the name is excluded from serialization.
     */
    public boolean hasExcludedName(String name) {
        return table.get(name) != null;
    }

    /**
     * Removes a name from the list of names to exclude.
     */
    public void removeExcludedName(String name) {
        table.remove(name);
    }

    /**
     * Adds the names of the standard objects and their 
     * prototypes to the list of excluded names.
     */
    public void excludeStandardObjectNames() {
        String[] names = { "Object", "Object.prototype",
                           "Function", "Function.prototype",
                           "String", "String.prototype",
                           "Math",  // no Math.prototype
                           "Array", "Array.prototype",
                           "Error", "Error.prototype",
                           "Number", "Number.prototype",
                           "Date", "Date.prototype",
                           "RegExp", "RegExp.prototype",
                           "Script", "Script.prototype"
                         };
        for (int i=0; i < names.length; i++) {
            addExcludedName(names[i]);
        }
    }

    static Object lookupQualifiedName(Scriptable scope,
                                      String qualifiedName) 
    {
        StringTokenizer st = new StringTokenizer(qualifiedName, ".");
        Object result = scope;
        while (st.hasMoreTokens()) {
            String s = st.nextToken();
            result = ((Scriptable)result).get(s, (Scriptable)result);
            if (result == null || !(result instanceof Scriptable))
                break;
        }
        return result;
    }

    static class PendingLookup implements Serializable { 
        PendingLookup(String name) { this.name = name; }
        String getName() { return name; }
        private String name;
    };

    protected Object replaceObject(Object obj) 
        throws IOException
    {
        String name = (String) table.get(obj);
        if (name == null)
            return obj;
        return new PendingLookup(name);
    }

    private Scriptable scope;
    private Hashtable table;
}
