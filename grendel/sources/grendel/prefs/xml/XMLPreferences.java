/*
 * PrefsWDefaults.java
 *
 * Created on 15 August 2005, 14:35
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.prefs.xml;

/**
 *
 * @author hash9
 */
public class XMLPreferences extends BasePrefs {
    private BasePrefs defaults = new BasePrefs();
    
    public XMLPreferences() {
        super();
    }
    
    public XMLPreferences(XMLPreferences children) {
        super(children);
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found.
     *
     * @param   key   the property key.
     * @return  the value in this property list with the specified key value.
     * @see     #setProperty
     * @see     #defaults
     */
    public Object getProperty(String key) {
        Object retValue;
        retValue = super.getProperty(key);
        if (retValue == null) {
            retValue = defaults.getProperty(key);
        }
        return retValue;
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>false</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     *
     *
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public boolean getPropertyBoolean(String key) {
        Boolean retValue;
        retValue = super.getProperty_Boolean(key);
        if (retValue == null) {
            retValue = defaults.getProperty_Boolean(key);
        }
        if (retValue == null) {
            return false;
        }
        return retValue;
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>0</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     *
     *
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public int getPropertyInt(String key) {
        Integer retValue;
        retValue = super.getProperty_Int(key);
        if (retValue == null) {
            retValue = defaults.getProperty_Int(key);
        }
        if (retValue == null) {
            return 0;
        }
        return retValue.intValue();
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>XMLPreferences</code> object.
     *
     *
     *
     *
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public Number getPropertyNumber(String key) {
        Number retValue;
        retValue = super.getPropertyNumber(key);
        if (retValue == null) {
            retValue = defaults.getPropertyNumber(key);
        }
        return retValue;
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>XMLPreferences</code> object.
     *
     *
     *
     *
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public XMLPreferences getPropertyPrefs(String key) {
        XMLPreferences retValue;
        retValue=(XMLPreferences) super.getProperty_Prefs(key);
        if (retValue == null) {
            retValue = (XMLPreferences) defaults.getProperty_Prefs(key);
        }
        if (retValue==null) {
            setProperty(key, new XMLPreferences());
            return getPropertyPrefs(key);
        }
        return retValue;
    }
    
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found.
     *
     * @param   key   the property key.
     * @return  the value in this property list with the specified key value.
     * @see     #setProperty
     * @see     #defaults
     */
    public String getPropertyString(String key) {
        String retValue;
        retValue = super.getPropertyString(key);
        if (retValue==null) {
            retValue = defaults.getPropertyString(key);
        }
        return retValue;
    }

    /**
     * Calls the <tt>Hashtable</tt> method <code>put</code>. Provided for
     * parallelism with the <tt>getProperty</tt> method. Enforces use of
     * strings for property keys and values. The value returned is the
     * result of the <tt>Hashtable</tt> call to <code>put</code>.
     * @param key the key to be placed into this property list.
     * @param value the value corresponding to <tt>key</tt>.
     * @see #getProperty
     * @since 1.2
     */
    public void setProperty(String key, Boolean value) {
        super.setProperty(key, value);
    }
    
    
    
    /**
     * Calls the <tt>Hashtable</tt> method <code>put</code>. Provided for
     * parallelism with the <tt>getProperty</tt> method. Enforces use of
     * strings for property keys and values. The value returned is the
     * result of the <tt>Hashtable</tt> call to <code>put</code>.
     * @param key the key to be placed into this property list.
     * @param value the value corresponding to <tt>key</tt>.
     * @see #getProperty
     * @since 1.2
     */
    public void setProperty(String key, Number value) {
        super.setProperty(key, value);
    }
    
    /**
     * Calls the <tt>Hashtable</tt> method <code>put</code>. Provided for
     * parallelism with the <tt>getProperty</tt> method. Enforces use of
     * strings for property keys and values. The value returned is the
     * result of the <tt>Hashtable</tt> call to <code>put</code>.
     * @param key the key to be placed into this property list.
     * @param value the value corresponding to <tt>key</tt>.
     * @see #getProperty
     * @since 1.2
     */
    public void setProperty(String key, XMLPreferences value) {
        super.setProperty(key, value);
    }
    
    /**
     * Returns a string representation of this <tt>Hashtable</tt> object
     * in the form of a set of entries, enclosed in braces and separated
     * by the ASCII characters "<tt>,&nbsp;</tt>" (comma and space). Each
     * entry is rendered as the key, an equals sign <tt>=</tt>, and the
     * associated element, where the <tt>toString</tt> method is used to
     * convert the key and element to strings. <p>Overrides to
     * <tt>toString</tt> method of <tt>Object</tt>.
     *
     * @return  a string representation of this hashtable.
     */
    public String toString() {
        String retValue;
        retValue = super.toString();
        return retValue;
    }
    
    /**
     * Calls the <tt>Hashtable</tt> method <code>put</code>. Provided for
     * parallelism with the <tt>getProperty</tt> method. Enforces use of
     * strings for property keys and values. The value returned is the
     * result of the <tt>Hashtable</tt> call to <code>put</code>.
     * @param key the key to be placed into this property list.
     * @param value the value corresponding to <tt>key</tt>.
     * @see #getProperty
     * @since 1.2
     */
    public void setProperty(String key, String value) {
        super.setProperty(key, value);
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns the
     * default value argument if the property is not found.
     *
     * @param   key            the hashtable key.
     * @param   defaultValue   a default value.
     *
     * @return  the value in this property list with the specified key value.
     * @see     #setProperty
     * @see     #defaults
     */
    public String getProperty(String key, String defaultValue) {
        String retValue;
        retValue = super.getProperty(key, defaultValue);
        return retValue;
    }
    
    /**
     * Emits an XML document representing all of the properties contained
     * in this table.
     * @param os the output stream on which to emit the XML document.
     * @see #loadFromXML(InputStream)
     * @since 1.5
     * @throws IOException if writing to the specified output stream
     *         results in an <tt>IOException</tt>.
     * @throws NullPointerException if <code>os</code> is null.
     */
    public void storeToXML(java.io.Writer w) throws java.io.IOException {
        super.storeToXML(w);
    }
    
    /**
     * Loads all of the properties represented by the XML document on the
     * specified input stream into this properties table.
     *
     * <p>The XML document must have the following DOCTYPE declaration:
     * <pre>
     * &lt;!DOCTYPE properties SYSTEM "http://java.sun.com/dtd/properties.dtd"&gt;
     * </pre>
     * Furthermore, the document must satisfy the properties DTD described
     * above.
     *
     * <p>The specified stream remains open after this method returns.
     *
     * @param in the input stream from which to read the XML document.
     * @throws IOException if reading from the specified input stream
     *         results in an <tt>IOException</tt>.
     * @throws InvalidPropertiesFormatException Data on input stream does not
     *         constitute a valid XML document with the mandated document type.
     * @throws NullPointerException if <code>in</code> is null.
     * @see    #storeToXML(OutputStream, String, String)
     * @since 1.5
     */
    public void loadFromXML(java.io.Reader r) throws java.io.IOException {
        super.loadFromXML(r);
    }
    
    /**
     * Loads all of the properties represented by the XML document on the
     * specified input stream into this properties table.
     *
     * <p>The XML document must have the following DOCTYPE declaration:
     * <pre>
     * &lt;!DOCTYPE properties SYSTEM "http://java.sun.com/dtd/properties.dtd"&gt;
     * </pre>
     * Furthermore, the document must satisfy the properties DTD described
     * above.
     *
     * <p>The specified stream remains open after this method returns.
     *
     * @param in the input stream from which to read the XML document.
     * @throws IOException if reading from the specified input stream
     *         results in an <tt>IOException</tt>.
     * @throws InvalidPropertiesFormatException Data on input stream does not
     *         constitute a valid XML document with the mandated document type.
     * @throws NullPointerException if <code>in</code> is null.
     * @see    #storeToXML(OutputStream, String, String)
     * @since 1.5
     */
    public void loadDefaultsFromXML(java.io.Reader r) throws java.io.IOException {
        defaults.loadFromXML(r);
    }
}
