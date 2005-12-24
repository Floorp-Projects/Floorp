/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Prefs.java
 *
 * Created on 09 August 2005, 20:30
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.prefs.xml;

import org.apache.xml.serialize.OutputFormat;
import org.apache.xml.serialize.XMLSerializer;

import org.xml.sax.ContentHandler;
import org.xml.sax.DTDHandler;
import org.xml.sax.ErrorHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.AttributesImpl;
import org.xml.sax.helpers.DefaultHandler;
import org.xml.sax.helpers.XMLReaderFactory;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;

import java.util.HashMap;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.Stack;


/**
 *
 * @author hash9
 */
class BasePrefs extends Hashtable<String,Object>
{
    /**
     * Creates a new instance of BasePrefs
     */
    public BasePrefs() {
        super();
    }
    
    public BasePrefs(Map<String, BasePrefs> children) {
        super(children);
    }
    
    public BasePrefs(BasePrefs encapsulate) {
        putAll(encapsulate);
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
        super.put(key, value);
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
    public void setProperty(String key, BasePrefs value) {
        super.put(key, value);
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
        super.put(key, value);
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
        super.put(key, value);
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
        return super.get(key);
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
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            retValue=defaultValue;
        }
        
        return retValue.toString();
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
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            return false;
        } else if (retValue instanceof Boolean) {
            return ((Boolean) retValue).booleanValue();
        } else if (retValue instanceof String) {
            return Boolean.parseBoolean(retValue.toString());
        } else {
            return false;
        }
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    protected Boolean getProperty_Boolean(String key) {
        Object retValue;
        retValue=super.get(key);
        if (retValue==null) {
            return null;
        } else if (retValue instanceof Boolean) {
            return ((Boolean) retValue).booleanValue();
        } else if (retValue instanceof String) {
            return Boolean.parseBoolean(retValue.toString());
        } else {
            return null;
        }
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>0/code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public int getPropertyInt(String key) {
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            return 0;
        } else if (retValue instanceof Number) {
            return ((Number) retValue).intValue();
        } else if (retValue instanceof String) {
            try {
                return Integer.parseInt(retValue.toString());
            } catch (NumberFormatException nfe) {
                return 0;
            }
        } else {
            return 0;
        }
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    protected Integer getProperty_Int(String key) {
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            return null;
        } else if (retValue instanceof Number) {
            return ((Integer) retValue);
        } else if (retValue instanceof String) {
            try {
                return new Integer(retValue.toString());
            } catch (NumberFormatException nfe) {
                return null;
            }
        } else {
            return null;
        }
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>Number</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public Number getPropertyNumber(String key) {
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            return null;
        } else if (retValue instanceof Number) {
            return (Number) retValue;
        } else if (retValue instanceof String) {
            try {
                return new Integer(retValue.toString());
            } catch (NumberFormatException nfe) {
                return new Integer(0);
            }
        } else {
            return null;
        }
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public BasePrefs getPropertyPrefs(String key) {
        BasePrefs retValue;
        retValue=this.getProperty_Prefs(key);
        if (retValue==null) {
            setProperty(key, new BasePrefs());
            return getPropertyPrefs(key);
        }
        return retValue;
    }
    
    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found, or isn't a
     * <code>BasePrefs</code> object.
     * 
     * 
     * @param key   the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    protected BasePrefs getProperty_Prefs(String key) {
        Object retValue;
        retValue=super.get(key);
        if (retValue==null) {
            return null;
        } else if (retValue instanceof BasePrefs) {
            return (BasePrefs) retValue;
        } else {
            return null;
        }
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
        Object retValue;
        retValue=super.get(key);
        
        if (retValue==null) {
            return null;
        } else {
            return retValue.toString();
        }
    }
    
    /**
     * Returns the value to which the specified key is mapped in this hashtable.
     * @param key a key in the hashtable.
     * @return the value to which the key is mapped in this hashtable;
     *          <code>null</code> if the key is not mapped to any value in
     *          this hashtable.
     * @see #put(Object, Object)
     * @throws NullPointerException if the key is <code>null</code>.
     * @deprecated Do Not USE Violates Protection
     */
    public Object get(Object key) {
        return super.get(key);
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
    public void loadFromXML(Reader r) throws java.io.IOException {
        try {
      /*try {
          XStream xstream = new XStream();
          Prefs prefs = (Prefs) xstream.fromXML(new InputStreamReader(in));
          this.putAll(prefs);
      } catch (com.thoughtworks.xstream.converters.ConversionException ce) {
          throw new InvalidPropertiesFormatException(ce);
      }*/
            Prefs_Parser handler=new Prefs_Parser();
            XMLReader parser=XMLReaderFactory.createXMLReader();
            parser.setContentHandler(handler);
            parser.setDTDHandler(handler);
            parser.setErrorHandler(handler);
            parser.parse(new InputSource(r));
            
            BasePrefs prefs=handler.getPrefs();
            this.putAll(prefs);
        } catch (Exception se) {
            IOException ioe=new IOException("XML Error");
            ioe.initCause(se);
            throw ioe;
        }
    }
    
    /**
     * Maps the specified <code>key</code> to the specified
     * <code>value</code> in this hashtable. Neither the key nor the
     * value can be <code>null</code>. <p>
     *
     * The value can be retrieved by calling the <code>get</code> method
     * with a key that is equal to the original key.
     *
     * Associates the specified value with the specified key in this map.
     * If the map previously contained a mapping for this key, the old
     * value is replaced.
     * @param key key with which the specified value is to be associated.
     * @param value value to be associated with the specified key.
     * @return previous value associated with specified key, or <tt>null</tt>
     *                if there was no mapping for key.  A <tt>null</tt> return can
     *                also indicate that the HashMap previously associated
     *                <tt>null</tt> with the specified key.
     * @exception NullPointerException if the key or value is
     *               <code>null</code>.
     * @see Object#equals(Object)
     * @see #get(Object)
     * @deprecated Do Not USE Violates Protection
     */
    public Object put(String key, Object value) {
        return super.put(key, value);
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
    public void storeToXML(Writer w) throws java.io.IOException {
    /*
    XStream xstream = new XStream();
    xstream.toXML(this,new PrintWriter(os, true));
    os.flush();
     */
        try {
            OutputFormat of=new OutputFormat("XML", "ISO-8859-1", true);
            of.setIndent(1);
            of.setIndenting(true);
            //of.setDoctype(null, "settings.dtd");
            
            XMLSerializer serializer=new XMLSerializer(w, of);
            ContentHandler hd=serializer.asContentHandler();
            hd.startDocument();
            
            AttributesImpl atts=new AttributesImpl();
            hd.startElement("", "", "prefs", atts);
            
            HashMap<String,Object> refs=new HashMap<String,Object>();
            decend(hd, refs);
            hd.endElement("", "", "prefs");
            hd.endDocument();
            w.close();
        } catch (SAXException se) {
            IOException ioe=new IOException("XML Error");
            ioe.initCause(se);
            throw ioe;
        }
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
        StringBuffer buf=new StringBuffer();
        buf.append("{");
        
        Iterator<Entry<String,Object>> i=entrySet().iterator();
        boolean hasNext=i.hasNext();
        
        while (hasNext) {
            Entry<String,Object> e=i.next();
            String key=e.getKey();
            Object value=e.getValue();
            buf.append(key);
            buf.append("=");
            
            if (value==this) {
                buf.append("(this Map)");
            } else if (value instanceof BasePrefs) {
                buf.append(value.toString());
            } else {
                buf.append(value);
            }
            
            hasNext=i.hasNext();
            
            if (hasNext) {
                buf.append(", ");
            }
        }
        
        buf.append("}");
        
        return buf.toString();
    }
    
    private void decend(ContentHandler hd, HashMap<String,Object> refs)
    throws SAXException {
        AttributesImpl atts=new AttributesImpl();
        Set<String> keys=this.keySet();
        
        for (String key : keys) {
            Object value=super.get(key);
            atts.clear();
            atts.addAttribute("", "", "id", "CDATA", key);
            
            if (refs.containsValue(value)) {
                atts.addAttribute(
                        "", "", "ref", "CDATA",
                        Integer.toString(value.hashCode()));
                hd.startElement("", "", "element", atts);
            } else {
                if (value!=null) {
                    atts.addAttribute(
                            "", "", "guid", "CDATA",
                            Integer.toString(value.hashCode()));
                    refs.put(Integer.toString(value.hashCode()), value);
                    atts.addAttribute(
                            "", "", "type", "CDATA", value.getClass().getName());
                    hd.startElement("", "", "element", atts);
                    
                    if (value instanceof BasePrefs) {
                        ((BasePrefs) value).decend(hd, refs);
                    } else {
                        hd.characters(
                                value.toString().toCharArray(), 0,
                                value.toString().length());
                    }
                }
            }
            
            hd.endElement("", "", "element");
        }
    }
    
    class Prefs_Parser extends DefaultHandler implements ContentHandler,
            DTDHandler, ErrorHandler {
        private HashMap<String,Object> refs=new HashMap<String,Object>();
        private BasePrefs map=new BasePrefs();
        private Stack<BasePrefs> current=new Stack<BasePrefs>(); //= new Prefs();
        private Stack<Class> stack_class=new Stack<Class>();
        private Stack<String> stack_guid=new Stack<String>();
        private Stack<String> stack_id=new Stack<String>();
        private Stack<String> stack_type=new Stack<String>();
        private StringBuilder sb=new StringBuilder();
        
        public BasePrefs getPrefs() {
            return map;
        }
        
        /**
         * Receive notification of character data.
         *
         * <p>The Parser will call this method to report each chunk of
         * character data.  SAX parsers may return all contiguous character
         * data in a single chunk, or they may split it into several
         * chunks; however, all of the characters in any single event
         * must come from the same external entity so that the Locator
         * provides useful information.</p>
         *
         * <p>The application must not attempt to read from the array
         * outside of the specified range.</p>
         *
         * <p>Individual characters may consist of more than one Java
         * <code>char</code> value.  There are two important cases where this
         * happens, because characters can't be represented in just sixteen bits.
         * In one case, characters are represented in a <em>Surrogate Pair</em>,
         * using two special Unicode values. Such characters are in the so-called
         * "Astral Planes", with a code point above U+FFFF.  A second case involves
         * composite characters, such as a base character combining with one or
         * more accent characters. </p>
         *
         * <p> Your code should not assume that algorithms using
         * <code>char</code>-at-a-time idioms will be working in character
         * units; in some cases they will split characters.  This is relevant
         * wherever XML permits arbitrary characters, such as attribute values,
         * processing instruction data, and comments as well as in data reported
         * from this method.  It's also generally relevant whenever Java code
         * manipulates internationalized text; the issue isn't unique to XML.</p>
         *
         * <p>Note that some parsers will report whitespace in element
         * content using the {@link #ignorableWhitespace ignorableWhitespace}
         * method rather than this one (validating parsers <em>must</em>
         * do so).</p>
         *
         *
         * @param ch the characters from the XML document
         * @param start the start position in the array
         * @param length the number of characters to read from the array
         * @see #ignorableWhitespace
         * @see org.xml.sax.Locator
         * @throws org.xml.sax.SAXException any SAX exception, possibly
         *            wrapping another exception
         */
        public void characters(char[] ch, int start, int length)
        throws SAXException {
            String s=new String(ch, start, length);
            s=s.trim();
            
            if (s.length()!=0) {
                sb.append(s);
            }
        }
        
        /**
         * Receive notification of the end of an element.
         *
         * <p>The SAX parser will invoke this method at the end of every
         * element in the XML document; there will be a corresponding
         * {@link #startElement startElement} event for every endElement
         * event (even when the element is empty).</p>
         *
         * <p>For information on the names, see startElement.</p>
         *
         * @param uri the Namespace URI, or the empty string if the
         *        element has no Namespace URI or if Namespace
         *        processing is not being performed
         * @param localName the local name (without prefix), or the
         *        empty string if Namespace processing is not being
         *        performed
         * @param qName the qualified XML name (with prefix), or the
         *        empty string if qualified names are not available
         * @throws org.xml.sax.SAXException any SAX exception, possibly
         *            wrapping another exception
         */
        public void endElement(String uri, String localName, String qName)
        throws SAXException {
            if (localName.equalsIgnoreCase("element")) {
                String id=stack_id.pop();
                String guid=stack_guid.pop();
                String type=stack_type.pop();
                Class c=stack_class.pop();
                
                if (type.equals("Referance")) {
                    Object o=refs.get(guid);
                    
                    if (o!=null) {
                        current.peek().put(id, o);
                    } else {
                        System.err.println("Invalid Referance");
                    }
                } else {
                    if (BasePrefs.class.isAssignableFrom(c)) {
                        BasePrefs p=current.pop();
                        current.peek().put(id, p);
                        refs.put(guid, p);
                    } else {
                        String s=sb.toString();
                        sb.setLength(0);
                        
                        if (Number.class.isAssignableFrom(c)) {
                            Number n;
                            
                            try {
                                n=new Integer(s);
                            } catch (NumberFormatException nfe) {
                                try {
                                    n=new Double(s);
                                } catch (NumberFormatException nfe_1) {
                                    n=new Integer(0);
                                }
                            }
                            
                            current.peek().put(id, n);
                            refs.put(guid, n);
                        } else if (Boolean.class.isAssignableFrom(c)) {
                            Boolean b=new Boolean(s);
                            current.peek().put(id, b);
                            refs.put(guid, b);
                        } else {
                            current.peek().put(id, s);
                            refs.put(guid, s);
                        }
                    }
                }
            }
        }
        
        /**
         * Receive notification of the beginning of a document.
         *
         * <p>The SAX parser will invoke this method only once, before any
         * other event callbacks (except for {@link #setDocumentLocator
         * setDocumentLocator}).</p>
         *
         *
         * @see #endDocument
         * @throws org.xml.sax.SAXException any SAX exception, possibly
         *            wrapping another exception
         */
        public void startDocument() throws SAXException {
            current.push(map);
        }
        
        /**
         * Receive notification of the beginning of an element.
         *
         * <p>The Parser will invoke this method at the beginning of every
         * element in the XML document; there will be a corresponding
         * {@link #endElement endElement} event for every startElement event
         * (even when the element is empty). All of the element's content will be
         * reported, in order, before the corresponding endElement
         * event.</p>
         *
         * <p>This event allows up to three name components for each
         * element:</p>
         *
         * <ol>
         * <li>the Namespace URI;</li>
         * <li>the local name; and</li>
         * <li>the qualified (prefixed) name.</li>
         * </ol>
         *
         * <p>Any or all of these may be provided, depending on the
         * values of the <var>http://xml.org/sax/features/namespaces</var>
         * and the <var>http://xml.org/sax/features/namespace-prefixes</var>
         * properties:</p>
         *
         * <ul>
         * <li>the Namespace URI and local name are required when
         * the namespaces property is <var>true</var> (the default), and are
         * optional when the namespaces property is <var>false</var> (if one is
         * specified, both must be);</li>
         * <li>the qualified name is required when the namespace-prefixes property
         * is <var>true</var>, and is optional when the namespace-prefixes property
         * is <var>false</var> (the default).</li>
         * </ul>
         *
         * <p>Note that the attribute list provided will contain only
         * attributes with explicit values (specified or defaulted):
         * #IMPLIED attributes will be omitted.  The attribute list
         * will contain attributes used for Namespace declarations
         * (xmlns* attributes) only if the
         * <code>http://xml.org/sax/features/namespace-prefixes</code>
         * property is true (it is false by default, and support for a
         * true value is optional).</p>
         *
         * <p>Like {@link #characters characters()}, attribute values may have
         * characters that need more than one <code>char</code> value.  </p>
         *
         *
         * @param uri the Namespace URI, or the empty string if the
         *        element has no Namespace URI or if Namespace
         *        processing is not being performed
         * @param localName the local name (without prefix), or the
         *        empty string if Namespace processing is not being
         *        performed
         * @param qName the qualified name (with prefix), or the
         *        empty string if qualified names are not available
         * @param atts the attributes attached to the element.  If
         *        there are no attributes, it shall be an empty
         *        Attributes object.  The value of this object after
         *        startElement returns is undefined
         * @see #endElement
         * @see org.xml.sax.Attributes
         * @see org.xml.sax.helpers.AttributesImpl
         * @throws org.xml.sax.SAXException any SAX exception, possibly
         *            wrapping another exception
         */
        public void startElement(
                String uri, String localName, String qName,
                org.xml.sax.Attributes atts)
                throws SAXException {
            HashMap<String,String> attributes=new HashMap<String,String>();
            
            for (int i=0; i<atts.getLength(); i++) {
                attributes.put(atts.getLocalName(i), atts.getValue(i));
            }
            
            if (attributes.size()>0) {
                if (attributes.get("ref")!=null) {
                    stack_type.push("Referance");
                    stack_guid.push(attributes.get("ref"));
                    stack_id.push(attributes.get("id"));
                    stack_class.push(Object.class);
                } else {
                    try {
                        stack_type.push(attributes.get("type"));
                        
                        Class c=Class.forName(attributes.get("type"));
                        stack_class.push(c);
                        
                        if (BasePrefs.class.isAssignableFrom(c)) {
                            BasePrefs p=(BasePrefs) c.newInstance();
                            current.push(p);
                        }
                        
                        stack_id.push(attributes.get("id"));
                        stack_guid.push(attributes.get("guid"));
                    } catch (Exception e) {
                        throw new SAXException(e);
                    }
                }
            }
        }
    }
}
