/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ethan Hugg
 *   Terry Lucas
 *   Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript.xml.impl.xmlbeans;

import java.util.Vector;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

import org.apache.xmlbeans.XmlCursor;

class XMLList extends XMLObjectImpl implements Function
{
    static final long serialVersionUID = -4543618751670781135L;

    static class AnnotationList
    {
        private Vector v;


        AnnotationList ()
        {
            v = new Vector();
        }


        void add (XML.XScriptAnnotation n)
        {
            v.add(n);
        }


        XML.XScriptAnnotation item(int index)
        {
            return (XML.XScriptAnnotation)(v.get(index));
        }


        void remove (int index)
        {
            v.remove(index);
        }


        int length()
        {
            return v.size();
        }
    };


    // Fields
    private AnnotationList    _annos;

    private XMLObjectImpl targetObject = null;
    private javax.xml.namespace.QName targetProperty = null;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  Constructors
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     *
     */
    XMLList(XMLLibImpl lib)
    {
        super(lib, lib.xmlListPrototype);
        _annos = new AnnotationList();
    }

    /**
     *
     * @param inputObject
     */
    XMLList(XMLLibImpl lib, Object inputObject)
    {
        super(lib, lib.xmlListPrototype);
        String frag;

        if (inputObject == null || inputObject instanceof Undefined)
        {
            frag = "";
        }
        else if (inputObject instanceof XML)
        {
            XML xml = (XML) inputObject;

            _annos = new AnnotationList();
            _annos.add(xml.getAnnotation());
        }
        else if (inputObject instanceof XMLList)
        {
            XMLList xmll = (XMLList) inputObject;

            _annos = new AnnotationList();

            for (int i = 0; i < xmll._annos.length(); i++)
            {
                _annos.add(xmll._annos.item(i));
            }
        }
        else
        {
            frag = ScriptRuntime.toString(inputObject).trim();

            if (!frag.startsWith("<>"))
            {
                frag = "<>" + frag + "</>";
            }

            frag = "<fragment>" + frag.substring(2);
            if (!frag.endsWith("</>"))
            {
                throw ScriptRuntime.typeError("XML with anonymous tag missing end anonymous tag");
            }

            frag = frag.substring(0, frag.length() - 3) + "</fragment>";

            XML orgXML = XML.createFromJS(lib, frag);

            // Now orphan the children and add them to our XMLList.
            XMLList children = orgXML.children();

            _annos = new AnnotationList();

            for (int i = 0; i < children._annos.length(); i++)
            {
                // Copy here is so that they'll be orphaned (parent() will be undefined)
                _annos.add(((XML) children.item(i).copy()).getAnnotation());
            }
        }
    }

    //
    //
    // TargetObject/Property accessors
    //
    //

    /**
     *
     * @param object
     * @param property
     */
    void setTargets(XMLObjectImpl object, javax.xml.namespace.QName property)
    {
        targetObject = object;
        targetProperty = property;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  Private functions
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     *
     * @param index
     * @return
     */
    XML getXmlFromAnnotation(int index)
    {
        XML retVal;

        if (index >= 0 && index < length())
        {
            XML.XScriptAnnotation anno = _annos.item(index);
            retVal = XML.getFromAnnotation(lib, anno);
        }
        else
        {
            retVal = null;
        }

        return retVal;
    }

    /**
     *
     * @param index
     */
    private void internalRemoveFromList (int index)
    {
        _annos.remove(index);
    }

    /**
     *
     * @param index
     * @param xml
     */
    void replace(int index, XML xml)
    {
        if (index < length())
        {
            AnnotationList newAnnoList = new AnnotationList();

            // Copy upto item to replace.
            for (int i = 0; i < index; i++)
            {
                newAnnoList.add(_annos.item(i));
            }

            newAnnoList.add(xml.getAnnotation());

            // Skip over old item we're going to replace we've already add new item on above line.
            for (int i = index + 1; i < length(); i++)
            {
                newAnnoList.add(_annos.item(i));
            }

            _annos = newAnnoList;
        }
    }

    /**
     *
     * @param index
     * @param xml
     */
    private void insert(int index, XML xml)
    {
        if (index < length())
        {
            AnnotationList newAnnoList = new AnnotationList();

            // Copy upto item to insert.
            for (int i = 0; i < index; i++)
            {
                newAnnoList.add(_annos.item(i));
            }

            newAnnoList.add(xml.getAnnotation());

            for (int i = index; i < length(); i++)
            {
                newAnnoList.add(_annos.item(i));
            }

            _annos = newAnnoList;
        }
    }

    //
    //
    //  methods overriding ScriptableObject
    //
    //

    public String getClassName ()
    {
        return "XMLList";
    }

    //
    //
    //  methods overriding IdScriptableObject
    //
    //

    /**
     *
     * @param index
     * @param start
     * @return
     */
    public Object get(int index, Scriptable start)
    {
        //Log("get index: " + index);

        if (index >= 0 && index < length())
        {
            return getXmlFromAnnotation(index);
        }
        else
        {
            return Scriptable.NOT_FOUND;
        }
    }

    /**
     *
     * @param name
     * @param start
     * @return
     */
    boolean hasXMLProperty(XMLName xmlName)
    {
        boolean result = false;

        // Has now should return true if the property would have results > 0 or
        // if it's a method name
        String name = xmlName.localName();
        if ((getPropertyList(xmlName).length() > 0) ||
            (getMethod(name) != NOT_FOUND))
        {
            result = true;
        }

        return result;
    }


    /**
     *
     * @param index
     * @param start
     * @return
     */
    public boolean has(int index, Scriptable start)
    {
        return 0 <= index && index < length();
    }

    /**
     *
     * @param name
     * @param value
     */
    void putXMLProperty(XMLName xmlName, Object value)
    {
        //Log("put property: " + name);

        // Special-case checks for undefined and null
        if (value == null)
        {
            value = "null";
        }
        else if (value instanceof Undefined)
        {
            value = "undefined";
        }

        if (length() > 1)
        {
            throw ScriptRuntime.typeError("Assignment to lists with more that one item is not supported");
        }
        else if (length() == 0)
        {
            // Secret sauce for super-expandos.
            // We set an element here, and then add ourselves to our target.
            if (targetObject != null &&
                targetProperty != null &&
                !targetProperty.getLocalPart().equals("*"))
            {
                // Add an empty element with our targetProperty name and then set it.
                XML xmlValue = XML.createTextElement(lib, targetProperty, "");
                addToList(xmlValue);

                if(xmlName.isAttributeName())
                {
                    setAttribute(xmlName, value);
                }
                else
                {
                    XML xml = item(0);
                    xml.putXMLProperty(xmlName, value);

                    // Update the list with the new item at location 0.
                    replace(0, item(0));
                }

                // Now add us to our parent
                XMLName name2 = XMLName.formProperty(targetProperty.getNamespaceURI(), targetProperty.getLocalPart());
                targetObject.putXMLProperty(name2, this);
            }
            else
            {
                throw ScriptRuntime.typeError("Assignment to empty XMLList without targets not supported");
            }
        }
        else if(xmlName.isAttributeName())
        {
            setAttribute(xmlName, value);
        }
        else
        {
            XML xml = item(0);
            xml.putXMLProperty(xmlName, value);

            // Update the list with the new item at location 0.
            replace(0, item(0));
        }
    }

    /**
     *
     * @param name
     * @return
     */
    Object getXMLProperty(XMLName name)
    {
        return getPropertyList(name);
    }

    /**
     *
     * @param index
     * @param value
     */
    public void put(int index, Scriptable start, Object value)
    {
        Object parent = Undefined.instance;
        // Convert text into XML if needed.
        XMLObject xmlValue;

        // Special-case checks for undefined and null
        if (value == null)
        {
            value = "null";
        }
        else if (value instanceof Undefined)
        {
            value = "undefined";
        }

        if (value instanceof XMLObject)
        {
            xmlValue = (XMLObject) value;
        }
        else
        {
            if (targetProperty == null)
            {
                xmlValue = XML.createFromJS(lib, value.toString());
            }
            else
            {
                xmlValue = XML.createTextElement(lib, targetProperty, value.toString());
            }
        }

        // Find the parent
        if (index < length())
        {
            parent = item(index).parent();
        }
        else
        {
            // Appending
            parent = parent();
        }

        if (parent instanceof XML)
        {
            // found parent, alter doc
            XML xmlParent = (XML) parent;

            if (index < length())
            {
                // We're replacing the the node.
                XML xmlNode = getXmlFromAnnotation(index);

                if (xmlValue instanceof XML)
                {
                    xmlNode.replaceAll((XML) xmlValue);
                    replace(index, xmlNode);
                }
                else if (xmlValue instanceof XMLList)
                {
                    // Replace the first one, and add the rest on the list.
                    XMLList list = (XMLList) xmlValue;

                    if (list.length() > 0)
                    {
                        int lastIndexAdded = xmlNode.childIndex();
                        xmlNode.replaceAll(list.item(0));
                        replace(index, list.item(0));

                        for (int i = 1; i < list.length(); i++)
                        {
                            xmlParent.insertChildAfter(xmlParent.getXmlChild(lastIndexAdded), list.item(i));
                            lastIndexAdded++;
                            insert(index + i, list.item(i));
                        }
                    }
                }
            }
            else
            {
                // Appending
                xmlParent.appendChild(xmlValue);
                addToList(xmlParent.getXmlChild(index));
            }
        }
        else
        {
            // Don't all have same parent, no underlying doc to alter
            if (index < length())
            {
                XML xmlNode = XML.getFromAnnotation(lib, _annos.item(index));

                if (xmlValue instanceof XML)
                {
                    xmlNode.replaceAll((XML) xmlValue);
                    replace(index, xmlNode);
                }
                else if (xmlValue instanceof XMLList)
                {
                    // Replace the first one, and add the rest on the list.
                    XMLList list = (XMLList) xmlValue;

                    if (list.length() > 0)
                    {
                        xmlNode.replaceAll(list.item(0));
                        replace(index, list.item(0));

                        for (int i = 1; i < list.length(); i++)
                        {
                            insert(index + i, list.item(i));
                        }
                    }
                }
            }
            else
            {
                addToList(xmlValue);
            }
        }
    }


    /**
     *
     * @param name
     */
    void deleteXMLProperty(XMLName name)
    {
        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);

            if (xml.tokenType() == XmlCursor.TokenType.START)
            {
                xml.deleteXMLProperty(name);
            }
        }
    }

    /**
     *
     * @param index
     */
    public void delete(int index)
    {
        if (index >= 0 && index < length())
        {
            XML xml = getXmlFromAnnotation(index);

            xml.remove();

            internalRemoveFromList(index);
        }
    }


    /**
     *
     * @return
     */
    public Object[] getIds()
    {
        Object enumObjs[];

        if (prototypeFlag)
        {
            enumObjs = new Object[0];
        }
        else
        {
            enumObjs = new Object[length()];

            for (int i = 0; i < enumObjs.length; i++)
            {
                enumObjs[i] = new Integer(i);
            }
        }

        return enumObjs;
    }

    /**
     *
     * @return
     */
    public Object[] getIdsForDebug()
    {
        return getIds();
    }


    // XMLList will remove will delete all items in the list (a set delete) this differs from the XMLList delete operator.
    void remove ()
    {
        int nLen = length();
        for (int i = nLen - 1; i >= 0; i--)
        {
            XML xml = getXmlFromAnnotation(i);
            if (xml != null)
            {
                xml.remove();
                internalRemoveFromList(i);
            }
        }
    }

    /**
     *
     * @param index
     * @return
     */
    XML item (int index)
    {
        return _annos != null
            ? getXmlFromAnnotation(index) : XML.createEmptyXML(lib);
    }


    /**
     *
     * @param name
     * @param value
     */
    private void setAttribute (XMLName xmlName, Object value)
    {
        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);
            xml.setAttribute(xmlName, value);
        }
    }


    /**
     *
     * @param toAdd
     */
    void addToList(Object toAdd)
    {
        if (toAdd instanceof Undefined)
        {
            // Missing argument do nothing...
            return;
        }

        if (toAdd instanceof XMLList)
        {
            XMLList xmlSrc = (XMLList)toAdd;
            for (int i = 0; i < xmlSrc.length(); i++)
            {
                _annos.add((xmlSrc.item(i)).getAnnotation());
            }
        }
        else if (toAdd instanceof XML)
        {
            _annos.add(((XML)(toAdd)).getAnnotation());
        }
        else if (toAdd instanceof XML.XScriptAnnotation)
        {
            _annos.add((XML.XScriptAnnotation)toAdd);
        }
    }

    //
    //
    // Methods from section 12.4.4 in the spec
    //
    //

    /**
     *
     * @param toAdd
     */
    XML addNamespace(Namespace ns)
    {
        if(length() == 1)
        {
            return getXmlFromAnnotation(0).addNamespace(ns);
        }
        else
        {
            throw ScriptRuntime.typeError("The addNamespace method works only on lists containing one item");
        }
    }

    /**
     *
     * @param xml
     * @return
     */
    XML appendChild(Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).appendChild(xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The appendChild method works only on lists containing one item");
        }
    }

    /**
     *
     * @param attr
     * @return
     */
    XMLList attribute(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);
            result.addToList(xml.attribute(xmlName));
        }

        return result;
    }

    /**
     *
     * @return
     */
    XMLList attributes()
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);
            result.addToList(xml.attributes());
        }

        return result;
    }

    XMLList child(long index)
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            result.addToList(getXmlFromAnnotation(i).child(index));
        }

        return result;
    }

    XMLList child(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            result.addToList(getXmlFromAnnotation(i).child(xmlName));
        }

        return result;
    }

    /**
     *
     * @return
     */
    int childIndex()
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).childIndex();
        }
        else
        {
            throw ScriptRuntime.typeError("The childIndex method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    XMLList children()
    {
        Vector v = new Vector();

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);

            if (xml != null)
            {
                Object o = xml.children();
                if (o instanceof XMLList)
                {
                    XMLList childList = (XMLList)o;

                    int cChildren = childList.length();
                    for (int j = 0; j < cChildren; j++)
                    {
                        v.addElement(childList.item(j));
                    }
                }
            }
        }

        XMLList allChildren = new XMLList(lib);
        int sz = v.size();

        for (int i = 0; i < sz; i++)
        {
            allChildren.addToList(v.get(i));
        }

        return allChildren;
    }

    /**
     *
     * @return
     */
    XMLList comments()
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);

            result.addToList(xml.comments());
        }

        return result;
    }

    /**
     *
     * @param xml
     * @return
     */
    boolean contains(Object xml)
    {
        boolean result = false;

        for (int i = 0; i < length(); i++)
        {
            XML member = getXmlFromAnnotation(i);

            if (member.equivalentXml(xml))
            {
                result = true;
                break;
            }
        }

        return result;
    }

    /**
     *
     * @return
     */
    Object copy()
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);
            result.addToList(xml.copy());
        }

        return result;
    }

    /**
     *
     * @return
     */
    XMLList descendants(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);
            result.addToList(xml.descendants(xmlName));
        }

        return result;
    }

    /**
     *
     * @return
     */
    Object[] inScopeNamespaces()
    {
        if(length() == 1)
        {
            return getXmlFromAnnotation(0).inScopeNamespaces();
        }
        else
        {
            throw ScriptRuntime.typeError("The inScopeNamespaces method works only on lists containing one item");
        }
    }

    /**
     *
     * @param child
     * @param xml
     */
    XML insertChildAfter(Object child, Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).insertChildAfter(child, xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The insertChildAfter method works only on lists containing one item");
        }
    }

    /**
     *
     * @param child
     * @param xml
     */
    XML insertChildBefore(Object child, Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).insertChildAfter(child, xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The insertChildBefore method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    boolean hasOwnProperty(XMLName xmlName)
    {
        boolean hasProperty = false;

        if (prototypeFlag)
        {
            String property = xmlName.localName();
            hasProperty = (0 != findPrototypeId(property));
        }
        else
        {
            hasProperty = (getPropertyList(xmlName).length() > 0);
        }

        return hasProperty;
    }

    /**
     *
     * @return
     */
    boolean hasComplexContent()
    {
        boolean complexContent;
        int length = length();

        if (length == 0)
        {
            complexContent = false;
        }
        else if (length == 1)
        {
            complexContent = getXmlFromAnnotation(0).hasComplexContent();
        }
        else
        {
            complexContent = false;

            for (int i = 0; i < length; i++)
            {
                XML nextElement = getXmlFromAnnotation(i);
                if (nextElement.tokenType() == XmlCursor.TokenType.START)
                {
                    complexContent = true;
                    break;
                }
            }
        }

        return complexContent;
    }

    /**
     *
     * @return
     */
    boolean hasSimpleContent()
    {
        boolean simpleContent;
        int length = length();

        if (length == 0)
        {
            simpleContent = true;
        }
        else if (length == 1)
        {
            simpleContent = getXmlFromAnnotation(0).hasSimpleContent();
        }
        else
        {
            simpleContent = true;

            for (int i = 0; i < length; i++)
            {
                XML nextElement = getXmlFromAnnotation(i);
                if (nextElement.tokenType() == XmlCursor.TokenType.START)
                {
                    simpleContent = false;
                    break;
                }
            }
        }

        return simpleContent;
    }

    /**
     *
     * @return
     */
    int length()
    {
        int result = 0;

        if (_annos != null)
        {
            result = _annos.length();
        }

        return result;
    }

    /**
     *
     * @return
     */
    String localName()
    {
        if (length() == 1)
        {
            return name().localName();
        }
        else
        {
            throw ScriptRuntime.typeError("The localName method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    QName name()
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).name();
        }
        else
        {
            throw ScriptRuntime.typeError("The name method works only on lists containing one item");
        }
    }

    /**
     *
     * @param prefix
     * @return
     */
    Object namespace(String prefix)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).namespace(prefix);
        }
        else
        {
            throw ScriptRuntime.typeError("The namespace method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    Object[] namespaceDeclarations()
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).namespaceDeclarations();
        }
        else
        {
            throw ScriptRuntime.typeError("The namespaceDeclarations method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    Object nodeKind()
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).nodeKind();
        }
        else
        {
            throw ScriptRuntime.typeError("The nodeKind method works only on lists containing one item");
        }
    }

    /**
     *
     */
    void normalize()
    {
        for (int i = 0; i < length(); i++)
        {
            getXmlFromAnnotation(i).normalize();
        }
    }

    /**
     * If list is empty, return undefined, if elements have different parents return undefined,
     * If they all have the same parent, return that parent.
     *
     * @return
     */
    Object parent()
    {
        Object sameParent = Undefined.instance;

        if ((length() == 0) && (targetObject != null) && (targetObject instanceof XML))
        {
            sameParent = targetObject;
        }
        else
        {
            for (int i = 0; i < length(); i++)
            {
                Object currParent = getXmlFromAnnotation(i).parent();

                if (i == 0)
                {
                    // Set the first for the rest to compare to.
                    sameParent = currParent;
                }
                else if (sameParent != currParent)
                {
                    sameParent = Undefined.instance;
                    break;
                }
            }
        }

        // If everything in the list is the sameParent then return that as the parent.
        return sameParent;
    }

    /**
     *
     * @param xml
     * @return
     */
    XML prependChild(Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).prependChild(xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The prependChild method works only on lists containing one item");
        }
    }

    /**
     *
     * @return
     */
    Object processingInstructions(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            XML xml = getXmlFromAnnotation(i);

            result.addToList(xml.processingInstructions(xmlName));
        }

        return result;
    }

    /**
     *
     * @param name
     * @return
     */
    boolean propertyIsEnumerable(Object name)
    {
        long index;
        if (name instanceof Integer) {
            index = ((Integer)name).intValue();
        } else if (name instanceof Number) {
            double x = ((Number)name).doubleValue();
            index = (long)x;
            if (index != x) {
                return false;
            }
            if (index == 0 && 1.0 / x < 0) {
                // Negative 0
                return false;
            }
        } else {
            String s = ScriptRuntime.toString(name);
            index = ScriptRuntime.testUint32String(s);
        }
        return (0 <= index && index < length());
    }

    /**
     *
     * @param ns
     */
    XML removeNamespace(Namespace ns)
    {
        if(length() == 1)
        {
            return getXmlFromAnnotation(0).removeNamespace(ns);
        }
        else
        {
            throw ScriptRuntime.typeError("The removeNamespace method works only on lists containing one item");
        }
    }

    XML replace(long index, Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).replace(index, xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The replace method works only on lists containing one item");
        }
    }

    /**
     *
     * @param propertyName
     * @param xml
     * @return
     */
    XML replace(XMLName xmlName, Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).replace(xmlName, xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The replace method works only on lists containing one item");
        }
    }

    /**
     *
     * @param xml
     */
    XML setChildren(Object xml)
    {
        if (length() == 1)
        {
            return getXmlFromAnnotation(0).setChildren(xml);
        }
        else
        {
            throw ScriptRuntime.typeError("The setChildren method works only on lists containing one item");
        }
    }

    /**
     *
     * @param name
     */
    void setLocalName(String localName)
    {
        if (length() == 1)
        {
            getXmlFromAnnotation(0).setLocalName(localName);
        }
        else
        {
            throw ScriptRuntime.typeError("The setLocalName method works only on lists containing one item");
        }
    }

    /**
     *
     * @param name
     */
    void setName(QName qname)
    {
        if (length() == 1)
        {
            getXmlFromAnnotation(0).setName(qname);
        }
        else
        {
            throw ScriptRuntime.typeError("The setName method works only on lists containing one item");
        }
    }

    /**
     *
     * @param ns
     */
    void setNamespace(Namespace ns)
    {
        if (length() == 1)
        {
            getXmlFromAnnotation(0).setNamespace(ns);
        }
        else
        {
            throw ScriptRuntime.typeError("The setNamespace method works only on lists containing one item");
        }
    }

    /**
     *
     * * @return
     */
    XMLList text()
    {
        XMLList result = new XMLList(lib);

        for (int i = 0; i < length(); i++)
        {
            result.addToList(getXmlFromAnnotation(i).text());
        }

        return result;
    }

    /**
     *
     * @return
     */
    public String toString()
    {
        if (hasSimpleContent())
        {
            StringBuffer sb = new StringBuffer();

            for(int i = 0; i < length(); i++)
            {
                XML next = getXmlFromAnnotation(i);
                sb.append(next.toString());
            }

            return sb.toString();
        }
        else
        {
            return toXMLString(0);
        }
    }

    String toSource(int indent)
    {
        // XXX indent is ignored
        return "<>"+toXMLString(0)+"</>";
    }

    /**
     *
     * @return
     */
    String toXMLString(int indent)
    {
        StringBuffer sb = new StringBuffer();

        for(int i = 0; i < length(); i++)
        {
            if (i > 0)
            {
                sb.append('\n');
            }

            sb.append(getXmlFromAnnotation(i).toXMLString(indent));
        }

        return sb.toString();
    }

    /**
     *
     * @return
     */
    Object valueOf()
    {
        return this;
    }

    //
    // Other public Functions from XMLObject
    //

    /**
     *
     * @param target
     * @return
     */
    boolean equivalentXml(Object target)
    {
        boolean result = false;

        // Zero length list should equate to undefined
        if (target instanceof Undefined && length() == 0)
        {
            result = true;
        }
        else if (length() == 1)
        {
            result = getXmlFromAnnotation(0).equivalentXml(target);
        }
        else if (target instanceof XMLList)
        {
            XMLList otherList = (XMLList) target;

            if (otherList.length() == length())
            {
                result = true;

                for (int i = 0; i < length(); i++)
                {
                    if (!getXmlFromAnnotation(i).equivalentXml(otherList.getXmlFromAnnotation(i)))
                    {
                        result = false;
                        break;
                    }
                }
            }
        }

        return result;
    }

    /**
     *
     * @param name
     * @param start
     * @return
     */
    private XMLList getPropertyList(XMLName name)
    {
        XMLList propertyList = new XMLList(lib);
        javax.xml.namespace.QName qname = null;

        if (!name.isDescendants() && !name.isAttributeName())
        {
            // Only set the targetProperty if this is a regular child get
            // and not a descendant or attribute get
            qname = new javax.xml.namespace.QName(name.uri(), name.localName());
        }

        propertyList.setTargets(this, qname);

        for (int i = 0; i < length(); i++)
        {
            propertyList.addToList(
                getXmlFromAnnotation(i).getPropertyList(name));
        }

        return propertyList;
    }

    private Object applyOrCall(boolean isApply,
                               Context cx, Scriptable scope,
                               Scriptable thisObj, Object[] args)
    {
        String methodName = isApply ? "apply" : "call";
        if(!(thisObj instanceof XMLList) ||
           ((XMLList)thisObj).targetProperty == null)
            throw ScriptRuntime.typeError1("msg.isnt.function",
                                           methodName);

        return ScriptRuntime.applyOrCall(isApply, cx, scope, thisObj, args);
    }

    protected Object jsConstructor(Context cx, boolean inNewExpr,
                                   Object[] args)
    {
        if (args.length == 0) {
            return new XMLList(lib);
        } else {
            Object arg0 = args[0];
            if (!inNewExpr && arg0 instanceof XMLList) {
                // XMLList(XMLList) returns the same object.
                return arg0;
            }
            return new XMLList(lib, arg0);
        }
    }

    org.apache.xmlbeans.XmlObject getXmlObject()
    {
        if (length() == 1) {
            return getXmlFromAnnotation(0).getXmlObject();
        } else {
            throw ScriptRuntime.typeError("getXmlObject method works only on lists containing one item");
        }
    }

    /**
     * See ECMA 357, 11_2_2_1, Semantics, 3_e.
     */
    public Scriptable getExtraMethodSource(Context cx)
    {
        if (length() == 1) {
            return getXmlFromAnnotation(0);
        }
        return null;
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        // This XMLList is being called as a Function.
        // Let's find the real Function object.
        if(targetProperty == null)
            throw ScriptRuntime.notFunctionError(this);

        String methodName = targetProperty.getLocalPart();

        boolean isApply = methodName.equals("apply");
        if(isApply || methodName.equals("call"))
            return applyOrCall(isApply, cx, scope, thisObj, args);

        Callable method = ScriptRuntime.getElemFunctionAndThis(
                              this, methodName, cx);
        // Call lastStoredScriptable to clear stored thisObj
        // but ignore the result as the method should use the supplied
        // thisObj, not one from redirected call
        ScriptRuntime.lastStoredScriptable(cx);
        return method.call(cx, scope, thisObj, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
    {
        throw ScriptRuntime.typeError1("msg.not.ctor", "XMLList");
    }
}


