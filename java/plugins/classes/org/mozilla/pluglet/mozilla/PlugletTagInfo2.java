/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
package org.mozilla.pluglet.mozilla;
import java.util.*;

public interface PlugletTagInfo2 extends PlugletTagInfo  {
    public Properties getAttributes();
    public String getAttribute(String name);
    /* Get the type of the HTML tag that was used ot instantiate this
     * pluglet.  Currently supported tags are EMBED, APPLET and OBJECT.
     */
    public String getTagType();
    /* Get the complete text of the HTML tag that was
     * used to instantiate this pluglet
     */
    public String getTagText();
    public String getDocumentBase();
    /* Return an encoding whose name is specified in:
     * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
     */
    public String getDocumentEncoding();
    public String getAlignment();
    public int getWidth();
    public int getHeight();
    public int getBorderVertSpace();
    public int getBorderHorizSpace();
    /* Returns a unique id for the current document on which the
     * pluglet is displayed.
     */
    public int getUniqueID();
}
