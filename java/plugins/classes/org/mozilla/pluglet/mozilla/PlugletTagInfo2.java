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
 * This interface extends <code>PlugletTagInfo</code>, providing additional information
 * about <code>Pluglet</code> tags (elements).
 */
public interface PlugletTagInfo2 extends PlugletTagInfo  {
    public Properties getAttributes();
    public String getAttribute(String name);
    public Properties getParameters();
    public String getParameter(String name);
    /** Gets the HTML tag type associated with creation of the
      * <code>Pluglet</code> instance.  Possible types are <code>EMBED</code>, 
      * <code>APPLET</code>, and <code>OBJECT</code>.<p>
      * @return Returns a <code>String</code> for the tag type as described above.
      */
    public String getTagType();
    /** Gets the complete text of the HTML tag that was
      * used to instantiate this <code>Pluglet</code> instance.
      * <p>
      * @return Returns a <code>String</code> for the tag text as described above.
      */
    public String getTagText();
    /**
     * Gets the base for the document. The base, in conjunction with a
     * relative URL, specifies the absolute path to the document.
     *<p>
     * @return Returns a <code>String</code> representing the document base as 
     * described above.
     */
    public String getDocumentBase();
    /** Returns the character encoding used in the document (e.g., ASCSII, Big5
      * (Traditional Chinese), Cp1122 (IBM Estonia). For a list of possible
      * character encodings, see:<p>
      * <code>http://java.sun.com/products/jdk/1.1/docs/guide/<br>
      * intl/intl.doc.html#25303</code><p>
      * @return Returns a <code>String</code> for the document encoding as described
      * above.
      */
    public String getDocumentEncoding();
    /** Returns the alignment attribute in the tag.
      * <p>
      * @return Returns the alignment attribute as described above.
      */
    public String getAlignment();
    /** Returns an <code>int</code> value for the width attribute of the tag. 
      *<p>
      * @return Returns an integer value for the width as described above.
      */
    public int getWidth();
    /** Returns an <code>int</code> for the height attribute of the tag. 
      * <p>
      * @return Returns an integer value for the height as described above.
      */
    public int getHeight();
    /** Returns an <code>int</code> for the border vertical space attribute 
      * of the tag (e.g., <code>vspace</code> with <code>IMG</code> tag).
      * <p>
      * @return Returns an integer value for the border vertical space attribute.
     */
    public int getBorderVertSpace();
    /** Returns an <code>int</code> for the border horizontal space attribute
      * of the tag (e.g., <code>hspace</code> with <code>IMG</code> tag).
      * <p>
      * @return Returns an integer value for the border horizontal space attribute.
     */
    public int getBorderHorizSpace();
    /** Returns a unique ID for the current document in which the
      * <code>Pluglet</code> is displayed.
      * <p>
      * @return Returns an ID for the current document as described above.
      */
    public int getUniqueID();
}
