/* 
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
 */
package org.mozilla.pluglet.mozilla;
import java.util.*;

/**
 * This interface provides information about the HTML tag on the page.
 */
public interface PlugletTagInfo {
    /**
     * Returns all the name-value pairs found in the tag attributes.
     * <p>
     * @return Returns the attributes of an HTML tag as type 
     * <code>java.util.Properties</code>.
     */
    public Properties getAttributes();
    /**
     * Returns a value for a particular attribute. Returns <code>NULL</code> 
     * if the attribute does not exist.<p>
     * @param name This is the name of the attribute.<p>
     * @return Returns the value of the named attribute as a <code>String</code>.
     */
    public String getAttribute(String name);

    /**
     * Returns the DOM element corresponding to the tag which references
     * this pluglet in the document.
     * 
     * REMINDERS: 
     *       <ul>
     *       <li>  You need to have JavaDOM installed.
     *       (see http://www.mozilla.org/projects/blacwood/dom)    
     *       </li>
     *       <li>
     *       You need to cast return value to org.w3c.dom.Element
     *       </li>
     *       </ul>
     * @return Return org.w3c.dom.Element correspondig to the tag
     */
    public Object getDOMElement();
} 

