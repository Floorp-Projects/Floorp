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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
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

// API class

package org.mozilla.javascript;

/**
 * This class describes the support needed to implement security.
 * <p>
 * Three main pieces of functionality are required to implement
 * security for JavaScript. First, it must be possible to define
 * classes with an associated security context. (This security 
 * context may be any object that has meaning to an embedding;
 * for a client-side JavaScript embedding this would typically
 * be an origin URL and/or a digital certificate.) Next it 
 * must be possible to get the current class context so that
 * the implementation can determine securely which class is
 * requesting a privileged action. And finally, it must be 
 * possible to map a class back into a security context so that
 * additional classes may be defined with that security context.
 * <p>
 * These three pieces of functionality are encapsulated in the
 * SecuritySupport class.
 * <p>
 * Additionally, an embedding may provide filtering on the 
 * Java classes that are visible to scripts through the 
 * <code>visibleToScripts</code> method.
 *
 * @see org.mozilla.javascript.Context
 * @see java.lang.ClassLoader
 * @since 1.4 Release 2
 * @author Norris Boyd
 */
public interface SecuritySupport {

    /**
     * Define and load a Java class.
     * <p>
     * In embeddings that care about security, the securityDomain 
     * must be associated with the defined class such that a call to
     * <code>getSecurityDomain</code> with that class will return this security
     * context.
     * <p>
     * @param name the name of the class
     * @param data the bytecode of the class
     * @param securityDomain some object specifying the security
     *        context of the code that is defining this class.
     *        Embeddings that don't care about security may allow
     *        null here. This value propagated from the values passed
     *        into methods of Context that evaluate scripts.
     * @see java.lang.ClassLoader#defineClass
     */
    public Class defineClass(String name, byte[] data, 
                             Object securityDomain);
        
    /**
     * Get the current class Context.
     * <p> 
     * This functionality is supplied by SecurityManager.getClassContext,
     * but only one SecurityManager may be instantiated in a single JVM
     * at any one time. So implementations that care about security must
     * provide access to this functionality through this interface.
     * <p>
     * Note that the 0th entry of the returned array should be the class
     * of the caller of this method. So if this method is implemented by
     * calling SecurityManager.getClassContext, this method must allocate
     * a new, shorter array to return.
     */
    public Class[] getClassContext();
    
    /**
     * Return the security context associated with the given class. 
     * <p>
     * If <code>cl</code> is a class defined through a call to 
     * SecuritySupport.defineClass, then return the security 
     * context from that call. Otherwise return null.
     * @param cl a class potentially defined by defineClass
     * @return a security context object previously passed to defineClass
     */
    public Object getSecurityDomain(Class cl);
    
    /**
     * Return true iff the Java class with the given name should be exposed
     * to scripts.
     * <p>
     * An embedding may filter which Java classes are exposed through 
     * LiveConnect to JavaScript scripts.
     * @param fullClassName the full name of the class (including the package
     *                      name, with '.' as a delimiter). For example the 
     *                      standard string class is "java.lang.String"
     * @return whether or not to reveal this class to scripts
     */
    public boolean visibleToScripts(String fullClassName);
}
