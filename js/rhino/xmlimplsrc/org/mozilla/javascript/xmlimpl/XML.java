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
 * Ethan Hugg
 * Terry Lucas
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

import java.lang.reflect.Member;
import java.util.*;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

import org.apache.xmlbeans.XmlCursor;
import org.apache.xmlbeans.XmlCursor.XmlBookmark;
import org.apache.xmlbeans.XmlCursor.TokenType;
import org.apache.xmlbeans.XmlException;
import org.apache.xmlbeans.XmlObject;
import org.apache.xmlbeans.XmlOptions;

class XML extends XMLObjectImpl implements Wrapper
{
    final static class XScriptAnnotation extends XmlBookmark
    {
        javax.xml.namespace.QName _name;
        XML _xScriptXML;


        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //
        //  Constructurs
        //
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        XScriptAnnotation (XmlCursor curs)
        {
            _name = curs.getName();
        }

    }

    /**
     *
     */
    final static class NamespaceDeclarations
    {
        private int             _prefixIdx;
        private StringBuffer    _namespaceDecls;
        private String          _defaultNSURI;


        NamespaceDeclarations (XmlCursor curs)
        {
            _prefixIdx = 0;
            _namespaceDecls = new StringBuffer();

            skipNonElements(curs);
            _defaultNSURI = curs.namespaceForPrefix("");

            if (isAnyDefaultNamespace())
            {
                addDecl("", _defaultNSURI);
            }
        }


        private void addDecl (String prefix, String ns)
        {
            _namespaceDecls.append((prefix.length() > 0 ?
                                        "declare namespace " + prefix :
                                        "default element namespace") +
                                    " = \"" + ns + "\"" + "\n");
        }


        String getNextPrefix (String ns)
        {
            String prefix = "NS" + _prefixIdx++;

            _namespaceDecls.append("declare namespace " + prefix + " = " + "\"" + ns + "\"" + "\n");

            return prefix;
        }


        boolean isAnyDefaultNamespace ()
        {
            return _defaultNSURI != null ?_defaultNSURI.length() > 0 : false;
        }


        String getDeclarations()
        {
            return _namespaceDecls.toString();
        }
    }

    // Fields
    //static final XML prototype = new XML();
    private XScriptAnnotation _anno;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  Constructors
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     *
     * @param anno
     */
    private XML(XMLLibImpl lib, XScriptAnnotation anno)
    {
        super(lib, lib.xmlPrototype);
        _anno = anno;
        _anno._xScriptXML = this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  Public factories for creating a XScript XML object given an XBean cursor.
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    static XML createEmptyXML(XMLLibImpl lib)
    {
        XScriptAnnotation anno;

        XmlObject xo = XmlObject.Factory.newInstance();
        XmlCursor curs = xo.newCursor();
        try {
            anno = new XScriptAnnotation(curs);
            curs.setBookmark(anno);
        } finally {
            curs.dispose();
        }

        return new XML(lib, anno);
    }

    private static XML createXML (XMLLibImpl lib, XmlCursor curs)
    {
        if (curs.currentTokenType().isStartdoc())
        {
            curs.toFirstContentToken();
        }

        XScriptAnnotation anno = findAnnotation(curs);

        return new XML(lib, anno);
    }

    /**
     * Special constructor for making an attribute
     *
     */
    private static XML createAttributeXML(XMLLibImpl lib, XmlCursor cursor)
    {
        if (!cursor.isAttr())
            throw new IllegalArgumentException();

        XScriptAnnotation anno = new XScriptAnnotation(cursor);
        cursor.setBookmark(anno);

        return new XML(lib, anno);
    }


    /**
     *
     * @param qname
     * @param value
     * @return
     */
    static XML createTextElement(XMLLibImpl lib, javax.xml.namespace.QName qname, String value)
    {
        XScriptAnnotation anno;

        XmlObject xo = XmlObject.Factory.newInstance();
        XmlCursor cursor = xo.newCursor();
        try {
            cursor.toNextToken();

            cursor.beginElement(qname.getLocalPart(), qname.getNamespaceURI());
            //if(namespace.length() > 0)
            //    cursor.insertNamespace("", namespace);
            cursor.insertChars(value);

            cursor.toStartDoc();
            cursor.toNextToken();
            anno = new XScriptAnnotation(cursor);
            cursor.setBookmark(anno);
        } finally {
            cursor.dispose();
        }

        return new XML(lib, anno);
    }

    static XML createFromXmlObject(XMLLibImpl lib, XmlObject xo)
    {
        XScriptAnnotation anno;
        XmlCursor curs = xo.newCursor();
        try {
            anno = new XScriptAnnotation(curs);
            curs.setBookmark(anno);
        } finally {
            curs.dispose();
        }
        return new XML(lib, anno);
    }

    static XML createFromJS(XMLLibImpl lib, Object inputObject)
    {
        XmlObject xo;
        boolean isText = false;
        String frag;

        if (inputObject == null || inputObject == Undefined.instance) {
            frag = "";
        } else if (inputObject instanceof XMLObjectImpl) {
            // todo: faster way for XMLObjects?
            frag = ((XMLObjectImpl) inputObject).toXMLString(0);
        } else {
            if (inputObject instanceof Wrapper) {
                // Note: the check has to be done after the check for
                //  XMLObjectImpl as XML also implements Wrapper
                Object wrapped = ((Wrapper)inputObject).unwrap();
                if (wrapped instanceof XmlObject) {
                    return createFromXmlObject(lib, (XmlObject)wrapped);
                }
            }
            frag = ScriptRuntime.toString(inputObject);
        }

        if (frag.trim().startsWith("<>"))
        {
            throw ScriptRuntime.typeError("Invalid use of XML object anonymous tags <></>.");
        }

        if (frag.indexOf("<") == -1)
        {
            // Must be solo text node, wrap in XML fragment
            isText = true;
            frag = "<textFragment>" + frag + "</textFragment>";
        }

        XmlOptions options = new XmlOptions();

        if (lib.ignoreComments)
        {
            options.put(XmlOptions.LOAD_STRIP_COMMENTS);
        }

        if (lib.ignoreProcessingInstructions)
        {
            options.put(XmlOptions.LOAD_STRIP_PROCINSTS);
        }

        if (lib.ignoreWhitespace)
        {
            options.put(XmlOptions.LOAD_STRIP_WHITESPACE);
        }

        try
        {
            xo = XmlObject.Factory.parse(frag, options);

            // Apply the default namespace
            Context cx = Context.getCurrentContext();
            String defaultURI = lib.getDefaultNamespaceURI(cx);

            if(defaultURI.length() > 0)
            {
                XmlCursor cursor = xo.newCursor();
                boolean isRoot = true;
                while(!cursor.toNextToken().isEnddoc())
                {
                    if(!cursor.isStart()) continue;

                    // Check if this element explicitly sets the
                    // default namespace
                    boolean defaultNSDeclared = false;
                    cursor.push();
                    while(cursor.toNextToken().isAnyAttr())
                    {
                        if(cursor.isNamespace())
                        {
                            if(cursor.getName().getLocalPart().length() == 0)
                            {
                                defaultNSDeclared = true;
                                break;
                            }
                        }
                    }
                    cursor.pop();
                    if(defaultNSDeclared)
                    {
                        cursor.toEndToken();
                        continue;
                    }

                    // Check if this element's name is in no namespace
                    javax.xml.namespace.QName qname = cursor.getName();
                    if(qname.getNamespaceURI().length() == 0)
                    {
                        // Change the namespace
                        qname = new javax.xml.namespace.QName(defaultURI,
                                                              qname.getLocalPart());
                        cursor.setName(qname);
                    }

                    if(isRoot)
                    {
                        // Declare the default namespace
                        cursor.push();
                        cursor.toNextToken();
                        cursor.insertNamespace("", defaultURI);
                        cursor.pop();

                        isRoot = false;
                    }
                }
                cursor.dispose();
            }
        }
        catch (XmlException xe)
        {
/*
todo need to handle namespace prefix not found in XML look for namespace type in the scope change.

            String errorMsg = "Use of undefined namespace prefix: ";
            String msg = xe.getError().getMessage();
            if (msg.startsWith(errorMsg))
            {
                String prefix = msg.substring(errorMsg.length());
            }
*/
            String errMsg = xe.getMessage();
            if (errMsg.equals("error: Unexpected end of file after null"))
            {
                // Create an empty document.
                xo = XmlObject.Factory.newInstance();
            }
            else
            {
                throw ScriptRuntime.typeError(xe.getMessage());
            }
        }
        catch (Throwable e)
        {
            // todo: TLL Catch specific exceptions during parse.
            throw ScriptRuntime.typeError("Not Parsable as XML");
        }

        XmlCursor curs = xo.newCursor();
        if (curs.currentTokenType().isStartdoc())
        {
            curs.toFirstContentToken();
        }

        if (isText)
        {
            // Move it to point to the text node
            curs.toFirstContentToken();
        }

        XScriptAnnotation anno;
        try
        {
            anno = new XScriptAnnotation(curs);
            curs.setBookmark(anno);
        }
        finally
        {
            curs.dispose();
        }

        return new XML(lib, anno);
    }

    static XML getFromAnnotation(XMLLibImpl lib, XScriptAnnotation anno)
    {
        if (anno._xScriptXML == null)
        {
            anno._xScriptXML = new XML(lib, anno);
        }

        return anno._xScriptXML;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  Private functions:
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     *
     * @param curs
     * @return
     */
    private static TokenType skipNonElements (XmlCursor curs)
    {
        TokenType tt = curs.currentTokenType();
        while (tt.isComment() || tt.isProcinst())
        {
            tt = curs.toNextToken();
        }

        return tt;
    }

    /**
     *
     * @param curs
     * @return
     */
    protected static XScriptAnnotation findAnnotation(XmlCursor curs)
    {
        XmlBookmark anno = curs.getBookmark(XScriptAnnotation.class);
        if (anno == null)
        {
            anno = new XScriptAnnotation(curs);
            curs.setBookmark(anno);
        }

        return (XScriptAnnotation)anno;
    }

    /**
     *
     * @return
     */
    private XmlOptions getOptions()
    {
        XmlOptions options = new XmlOptions();

        if (lib.ignoreComments)
        {
            options.put(XmlOptions.LOAD_STRIP_COMMENTS);
        }

        if (lib.ignoreProcessingInstructions)
        {
            options.put(XmlOptions.LOAD_STRIP_PROCINSTS);
        }

        if (lib.ignoreWhitespace)
        {
            options.put(XmlOptions.LOAD_STRIP_WHITESPACE);
        }

        if (lib.prettyPrinting)
        {
            options.put(XmlOptions.SAVE_PRETTY_PRINT, null);
            options.put(XmlOptions.SAVE_PRETTY_PRINT_INDENT, new Integer(lib.prettyIndent));
        }

        return options;
    }


    /**
     *
     * @param cursor
     * @param opts
     * @return
     */
    private static String dumpNode(XmlCursor cursor, XmlOptions opts)
    {
        if (cursor.isText())
            return cursor.getChars();

        if (cursor.isFinish())
            return "";

        cursor.push();
        boolean wanRawText = cursor.isStartdoc() && !cursor.toFirstChild();
        cursor.pop();

        return wanRawText ? cursor.getTextValue() : cursor.xmlText( opts );
    }

    /**
     *
     * @return
     */
    private XmlCursor newCursor ()
    {
        XmlCursor curs;

        if (_anno != null)
        {
            curs = _anno.createCursor();
            if (curs == null)
            {
                // Orphaned case.
                XmlObject doc = XmlObject.Factory.newInstance();
                curs = doc.newCursor();

                if (_anno._name != null)
                {
                    curs.toNextToken();
                    curs.insertElement(_anno._name);
                    curs.toPrevSibling();
                }

                curs.setBookmark(_anno);
            }
        }
        else
        {
            XmlObject doc = XmlObject.Factory.newInstance();
            curs = doc.newCursor();
        }

        return curs;
    }

    /*
     * fUseStartDoc used by child(int index) the index is at startDoc is the element at the top-level
     *              otherwise we always want to drill in.
     */
    private boolean moveToChild(XmlCursor curs, long index, boolean fFirstChild, boolean fUseStartDoc)
    {
        if (index < 0)
            throw new IllegalArgumentException();

        long idxChild = 0;

        if (!fUseStartDoc && curs.currentTokenType().isStartdoc())
        {
            // We always move to the children of the top node.
            // todo:  This assumes that we want have multiple top-level nodes.  Which we should be able tohave.
            curs.toFirstContentToken();
        }

        TokenType tt = curs.toFirstContentToken();
        if (!tt.isNone() && !tt.isEnd())
        {
            while (true)
            {
                if (index == idxChild)
                {
                    return true;
                }

                tt = curs.currentTokenType();
                if (tt.isText())
                {
                    curs.toNextToken();
                }
                else if (tt.isStart())
                {
                    // Need to do this we want to be pointing at the text if that after the end token.
                    curs.toEndToken();
                    curs.toNextToken();
                }
                else if (tt.isComment() || tt.isProcinst())
                {
                    continue;
                }
                else
                {
                    break;
                }

                idxChild++;
            }
        }
        else if (fFirstChild && index == 0)
        {
            // Drill into where first child would be.
//            curs.toFirstContentToken();
            return true;
        }

        return false;
    }

    /**
     *
     * @return
     */
    XmlCursor.TokenType tokenType()
    {
        XmlCursor.TokenType result;

        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        result = curs.currentTokenType();

        curs.dispose();

        return result;
    }
    /**
     *
     * @param srcCurs
     * @param destCurs
     * @param fDontMoveIfSame
     * @return
     */
    private boolean moveSrcToDest (XmlCursor srcCurs, XmlCursor destCurs, boolean fDontMoveIfSame)
    {
        boolean fMovedSomething = true;
        TokenType tt;
        do
        {
            if (fDontMoveIfSame && srcCurs.isInSameDocument(destCurs) && (srcCurs.comparePosition(destCurs) == 0))
            {
                // If the source and destination are pointing at the same place then there's nothing to move.
                fMovedSomething = false;
                break;
            }

            // todo ***TLL*** Use replaceContents (when added) and eliminate children removes (see above todo).
            if (destCurs.currentTokenType().isStartdoc())
            {
                destCurs.toNextToken();
            }

            // todo ***TLL*** Can Eric support notion of copy instead of me copying then moving???
            XmlCursor copyCurs = copy(srcCurs);

            copyCurs.moveXml(destCurs);

            copyCurs.dispose();

            tt = srcCurs.currentTokenType();
        } while (!tt.isStart() && !tt.isEnd() && !tt.isEnddoc());

        return fMovedSomething;
    }

    /**
     *
     * @param cursToCopy
     * @return
     */
    private XmlCursor copy (XmlCursor cursToCopy)
    {
        XmlObject xo = XmlObject.Factory.newInstance();

        XmlCursor copyCurs = null;

        if (cursToCopy.currentTokenType().isText())
        {
            try
            {
                // Try just as a textnode, to do that we need to wrap the text in a special fragment tag
                // that is not visible from the XmlCursor.
                copyCurs = XmlObject.Factory.parse("<x:fragment xmlns:x=\"http://www.openuri.org/fragment\">" +
                                           cursToCopy.getChars() +
                                           "</x:fragment>").newCursor();
                if (!cursToCopy.toNextSibling())
                {
                    if (cursToCopy.currentTokenType().isText())
                    {
                        cursToCopy.toNextToken();   // It's not an element it's text so skip it.
                    }
                }
            }
            catch (Exception ex)
            {
                throw ScriptRuntime.typeError(ex.getMessage());
            }
        }
        else
        {
            copyCurs = xo.newCursor();
            copyCurs.toFirstContentToken();

            cursToCopy.copyXml(copyCurs);
            if (!cursToCopy.toNextSibling())        // If element skip element.
            {
                if (cursToCopy.currentTokenType().isText())
                {
                    cursToCopy.toNextToken();       // It's not an element it's text so skip it.
                }
            }

        }

        copyCurs.toStartDoc();
        copyCurs.toFirstContentToken();

        return copyCurs;
    }

    private static final int APPEND_CHILD = 1;
    private static final int PREPEND_CHILD = 2;

    /**
     *
     * @param curs
     * @param xmlToInsert
     */
    private void insertChild(XmlCursor curs, Object xmlToInsert)
    {
        if (xmlToInsert == null || xmlToInsert instanceof Undefined)
        {
            // Do nothing
        }
        else if (xmlToInsert instanceof XmlCursor)
        {
            moveSrcToDest((XmlCursor)xmlToInsert, curs, true);
        }
        else if (xmlToInsert instanceof XML)
        {
            XML xmlValue = (XML) xmlToInsert;

            // If it's an attribute, then change to text node
            if (xmlValue.tokenType() == XmlCursor.TokenType.ATTR)
            {
                insertChild(curs, xmlValue.toString());
            }
            else
            {
                XmlCursor cursToInsert = ((XML) xmlToInsert).newCursor();

                moveSrcToDest(cursToInsert, curs, true);

                cursToInsert.dispose();
            }
        }
        else if (xmlToInsert instanceof XMLList)
        {
            XMLList list = (XMLList) xmlToInsert;

            for (int i = 0; i < list.length(); i++)
            {
                insertChild(curs, list.item(i));
            }
        }
        else
        {
            // Convert to string and make XML out of it
            String  xmlStr = ScriptRuntime.toString(xmlToInsert);
            XmlObject xo = XmlObject.Factory.newInstance();         // Create an empty document.

            XmlCursor sourceCurs = xo.newCursor();
            sourceCurs.toNextToken();

            // To hold the text.
            sourceCurs.insertChars(xmlStr);

            sourceCurs.toPrevToken();

            // Call us again with the cursor.
            moveSrcToDest(sourceCurs, curs, true);
        }
    }

    /**
     *
     * @param childToMatch
     * @param xmlToInsert
     * @param addToType
     */
    private void insertChild(XML childToMatch, Object xmlToInsert, int addToType)
    {
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();
        XmlCursor xmlChildCursor = childToMatch.newCursor();

        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            tt = curs.toNextToken();

            while (!tt.isEnd())
            {
                if (tt.isStart())
                {
                    // See if this child is the same as the one thep passed in
                    if (curs.comparePosition(xmlChildCursor) == 0)
                    {
                        // Found it
                        if (addToType == APPEND_CHILD)
                        {
                            // Move the cursor to just past the end of this element
                            curs.toEndToken();
                            curs.toNextToken();
                        }

                        insertChild(curs, xmlToInsert);
                        break;
                    }
                }

                // Skip over child elements
                if (tt.isStart())
                {
                    tt = curs.toEndToken();
                }

                tt = curs.toNextToken();
            }

        }

        xmlChildCursor.dispose();
        curs.dispose();
    }

    /**
     *
     * @param curs
     */
    protected void removeToken (XmlCursor curs)
    {
        XmlObject xo = XmlObject.Factory.newInstance();

        // Don't delete anything move to another document so it gets orphaned nicely.
        XmlCursor tmpCurs = xo.newCursor();
        tmpCurs.toFirstContentToken();


        curs.moveXml(tmpCurs);

        tmpCurs.dispose();
    }

    /**
     *
     * @param index
     */
    protected void removeChild(long index)
    {
        XmlCursor curs = newCursor();

        if (moveToChild(curs, index, false, false))
        {
            removeToken(curs);
        }

        curs.dispose();
    }

    /**
     *
     * @param name
     * @return
     */
    protected static javax.xml.namespace.QName computeQName (Object name)
    {
        if (name instanceof String)
        {
            String ns = null;
            String localName = null;

            String fullName = (String)name;
            localName = fullName;
            if (fullName.startsWith("\""))
            {
                int idx = fullName.indexOf(":");
                if (idx != -1)
                {
                    ns = fullName.substring(1, idx - 1);    // Don't include the "" around the namespace
                    localName = fullName.substring(idx + 1);
                }
            }

            if (ns == null)
            {
                return new javax.xml.namespace.QName(localName);
            }
            else
            {
                return new javax.xml.namespace.QName(ns, localName);
            }
        }

        return null;
    }

    /**
     *
     * @param destCurs
     * @param newValue
     */
    private void replace(XmlCursor destCurs, XML newValue)
    {
        if (destCurs.isStartdoc())
        {
            // Can't overwrite a whole document (user really wants to overwrite the contents of).
            destCurs.toFirstContentToken();
        }

        // Orphan the token -- don't delete it outright on the XmlCursor.
        removeToken(destCurs);

        XmlCursor srcCurs = newValue.newCursor();
        if (srcCurs.currentTokenType().isStartdoc())
        {
            // Cann't append a whole document (user really wants to append the contents of).
            srcCurs.toFirstContentToken();
        }

        moveSrcToDest(srcCurs, destCurs, false);

        // Re-link a new annotation to this cursor -- we just deleted the previous annotation on entrance to replace.
        if (!destCurs.toPrevSibling())
        {
            destCurs.toPrevToken();
        }
        destCurs.setBookmark(new XScriptAnnotation(destCurs));

        // todo would be nice if destCurs.toNextSibling went to where the next token if the cursor was pointing at the last token in the stream.
        destCurs.toEndToken();
        destCurs.toNextToken();

        srcCurs.dispose();
    }

    /**
     *
     * @param currXMLNode
     * @param xmlValue
     * @return
     */
    private boolean doPut(XMLName name, XML currXMLNode, XMLObjectImpl xmlValue)
    {
        boolean result = false;
        XmlCursor curs = currXMLNode.newCursor();

        try
        {
            // Replace the node with this new xml value.
            XML xml;

            int toAssignLen = xmlValue.length();

            for (int i = 0; i < toAssignLen; i++)
            {
                if (xmlValue instanceof XMLList)
                {
                    xml = (XML)((XMLList) xmlValue).item(i);
                }
                else
                {
                    xml = (XML) xmlValue;
                }

                // If it's an attribute or text node, make text node.
                XmlCursor.TokenType tt = xml.tokenType();
                if (tt == XmlCursor.TokenType.ATTR || tt == XmlCursor.TokenType.TEXT)
                {
                    xml = makeXmlFromString(lib, name, xml.toString());
                }

                if (i == 0)
                {
                    // 1st assignment is replaceChild all others are appendChild
                    replace(curs, xml);
                }
                else
                {
                    insertChild(curs, xml);
                }
            }

            // We're done we've blown away the node because the rvalue was XML...
            result = true;
        }
        catch (Exception ex)
        {
            ex.printStackTrace();
            throw ScriptRuntime.typeError(ex.getMessage());
        }
        finally
        {
            curs.dispose();
        }

        return result;
    }

    /**
     * Make a text node element with this element name and text value.
     *
     * @param name
     * @param value
     * @return
     */
    private XML makeXmlFromString(XMLLibImpl lib, XMLName name,
                                      String value)
    {
        XML result;

        javax.xml.namespace.QName qname;

        try
        {
            qname = new javax.xml.namespace.QName(name.uri(), name.localName());
        }
        catch(Exception e)
        {
            throw ScriptRuntime.typeError(e.getMessage());
        }

        result = createTextElement(lib, qname, value.toString());

        return result;
    }

    /**
     *
     * @param name
     * @return
     */
    private XMLList matchAttributes(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);
        XmlCursor curs = newCursor();

        if (curs.currentTokenType().isStartdoc())
        {
            curs.toFirstContentToken();
        }

        if (curs.isStart())
        {
            if (curs.toFirstAttribute())
            {
                do
                {
                    if (qnameMatches(xmlName, curs.getName()))
                    {
                        result.addToList(createAttributeObject(curs));
                    }
                } while (curs.toNextAttribute());
            }
        }

        curs.dispose();

        return result;
    }

    /**
     *
     * @param attrCurs
     * @return
     */
    private XML createAttributeObject (XmlCursor attrCurs)
    {
        XML result = null;

        if (attrCurs.currentTokenType().isAttr())
        {
            result = createAttributeXML(lib, attrCurs);
        }

        return result;
    }

    //
    //
    //  methods overriding ScriptableObject
    //
    //

    public String getClassName ()
    {
        return "XML";
    }

    //
    //
    //  methods overriding IdScriptableObject
    //
    //

    /**
     * XML[0] should return this, all other indexes are Undefined
     *
     * @param index
     * @param start
     * @return
     */
    public Object get(int index, Scriptable start)
    {
        //Log("get index: " + index);

        if (index == 0)
        {
            return this;
        }
        else
        {
            return Scriptable.NOT_FOUND;
        }
    }

    /**
     * Does the named property exist
     *
     * @param name
     * @param start
     * @return
     */
    boolean hasXMLProperty(XMLName xmlName)
    {
        boolean result = false;

        if (prototypeFlag)
        {
            String name = xmlName.localName();

            if (getMethod(name) != NOT_FOUND)
            {
                result = true;
            }
        }
        else
        {
            // Has now should return true if the property would have results > 0 or
            // if it's a method name
            String name = xmlName.localName();
            if ((getPropertyList(xmlName).length() > 0) ||
                (getMethod(name) != NOT_FOUND))
            {
                result = true;
            }
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
        return (index == 0);
    }

    /**
     *
     * @return
     */
    public Object[] getIds()
    {
        Object[] enumObjs;

        if (prototypeFlag)
        {
            enumObjs = new Object[0];
        }
        else
        {
            enumObjs = new Object[1];

            enumObjs[0] = new Integer(0);
        }

        return enumObjs;
    }


    /**
     *
     * @return
     */
    public Object [] getIdsForDebug()
    {
        return getIds();
    }

    /**
     *
     * @param name
     * @param start
     * @return
     */
    Object getXMLProperty(XMLName xmlName)
    {
        Object result = NOT_FOUND;

        if (prototypeFlag)
        {
            String name = xmlName.localName();

            result = getMethod(name);
        }
        else
        {
            result = getPropertyList(xmlName);
        }

        return result;
    }

    /**
     *
     * @param name
     * @param start
     * @param value
     */
    void putXMLProperty(XMLName xmlName, Object value)
    {
        //Log("put property: " + name + " value: " + value.getClass());

        if (prototypeFlag)
        {
        }
        else
        {
            // Special-case checks for undefined and null
            if (value == null)
            {
                value = "null";
            }
            else if (value instanceof Undefined)
            {
                value = "undefined";
            }

            // Get the named property
            if (xmlName.isAttributeName())
            {
                setAttribute(xmlName, value);
            }
            else if (xmlName.uri() == null &&
                     xmlName.localName().equals("*"))
            {
                setChildren(value);
            }
            else
            {
                // Convert text into XML if needed.
                XMLObjectImpl xmlValue = null;

                if (value instanceof XMLObjectImpl)
                {
                    xmlValue = (XMLObjectImpl) value;

                    // Check for attribute type and convert to textNode
                    if (xmlValue instanceof XML)
                    {
                        if (((XML) xmlValue).tokenType() == XmlCursor.TokenType.ATTR)
                        {
                            xmlValue = makeXmlFromString(lib, xmlName, xmlValue.toString());
                        }
                    }

                    if (xmlValue instanceof XMLList)
                    {
                        for (int i = 0; i < xmlValue.length(); i++)
                        {
                            XML xml = (XML)((XMLList) xmlValue).item(i);

                            if (xml.tokenType() == XmlCursor.TokenType.ATTR)
                            {
                                ((XMLList) xmlValue).replace(i, makeXmlFromString(lib, xmlName, xml.toString()));
                            }
                        }
                    }
                }
                else
                {
                    xmlValue = makeXmlFromString(lib, xmlName, ScriptRuntime.toString(value));
                }

                XMLList matches = (XMLList)getPropertyList(xmlName);

                if (matches.length() == 0)
                {
                    appendChild(xmlValue);
                }
                else
                {
                    // Remove all other matches
                    for (int i = 1; i < matches.length(); i++)
                    {
                        removeChild(matches.item(i).childIndex());
                    }

                    // Replace first match with new value.
                    doPut(xmlName, (XML)matches.item(0), xmlValue);
                }
            }
        }
    }


    /**
     *
     * @param index
     * @param start
     * @param value
     */
    public void put(int index, Scriptable start, Object value)
    {
        // Spec says assignment to indexed XML object should return type error
        throw ScriptRuntime.typeError("Assignment to indexed XML is not allowed");
    }


    /**
     *
     * @param name
     */
    void deleteXMLProperty(XMLName name)
    {
        if (!name.isDescendants() && name.isAttributeName())
        {
            XmlCursor curs = newCursor();

            // TODO: Cover the case *::name
            if (name.localName().equals("*"))
            {
                // Delete all attributes.
                if (curs.toFirstAttribute())
                {
                    while (curs.currentTokenType().isAttr())
                    {
                        curs.removeXml();
                    }
                }
            }
            else
            {
                // Delete an attribute.
                javax.xml.namespace.QName qname = new javax.xml.namespace.QName(
                    name.uri(), name.localName());
                curs.removeAttribute(qname);
            }

            curs.dispose();
        }
        else
        {
            XMLList matches = (XMLList)getPropertyList(name);

            matches.remove();
        }
    }


    /**
     *
     * @param index
     */
    public void delete(int index)
    {
        if (index == 0)
        {
            remove();
        }
    }

    //
    //
    //  package utility functions:
    //
    //

    protected XScriptAnnotation getAnnotation ()
    { return _anno; }


    protected void changeNS (String oldURI, String newURI)
    {
        XmlCursor curs = newCursor();
        while (curs.toParent())
            /* Goto the top of the document */;

        TokenType tt = curs.currentTokenType();
        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isStart())
        {
            do
            {
                if (tt.isStart() || tt.isAttr() || tt.isNamespace())
                {
                    javax.xml.namespace.QName currQName = curs.getName();
                    if (oldURI.equals(currQName.getNamespaceURI()))
                    {
                        curs.setName(new javax.xml.namespace.QName(newURI, currQName.getLocalPart()));
                    }
                }

                tt = curs.toNextToken();
            } while (!tt.isEnddoc() && !tt.isNone());
        }

        curs.dispose();
    }


    /**
     *
     */
    void remove ()
    {
        XmlCursor childCurs = newCursor();

        if (childCurs.currentTokenType().isStartdoc())
        {
            // Remove on the document removes all children.
            TokenType tt = childCurs.toFirstContentToken();
            while (!tt.isEnd() && !tt.isEnddoc())
            {
                removeToken(childCurs);
                tt = childCurs.currentTokenType();      // Now see where we're pointing after the delete -- next token.
            }
        }
        else
        {
                removeToken(childCurs);
        }

        childCurs.dispose();
    }


    /**
     *
     * @param value
     */
    void replaceAll(XML value)
    {
        XmlCursor curs = newCursor();

        replace(curs, value);
        _anno = value._anno;

        curs.dispose();
    }


    /**
     *
     * @param attrName
     * @param value
     */
    void setAttribute(XMLName xmlName, Object value)
    {
        if (xmlName.uri() == null &&
            xmlName.localName().equals("*"))
        {
            throw ScriptRuntime.typeError("@* assignment not supported.");
        }

        XmlCursor curs = newCursor();

        String strValue = ScriptRuntime.toString(value);
        if (curs.currentTokenType().isStartdoc())
        {
            curs.toFirstContentToken();
        }

        javax.xml.namespace.QName qName;

        try
        {
            qName = new javax.xml.namespace.QName(xmlName.uri(), xmlName.localName());
        }
        catch(Exception e)
        {
            throw ScriptRuntime.typeError(e.getMessage());
        }

        if (!curs.setAttributeText(qName, strValue))
        {
            if (curs.currentTokenType().isStart())
            {
                // Can only add attributes inside of a start.
                curs.toNextToken();
            }
            curs.insertAttributeWithValue(qName, strValue);
        }

        curs.dispose();
    }

    /**
     *
     * @param namespace
     * @return
     */
    private XMLList allChildNodes(String namespace)
    {
        XMLList result = new XMLList(lib);
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();
        javax.xml.namespace.QName targetProperty = new javax.xml.namespace.QName(namespace, "*");

        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            tt = curs.toFirstContentToken();

            while (!tt.isEnd())
            {
                if (!tt.isStart())
                {
                    // Not an element
                    result.addToList(findAnnotation(curs));

                    // Reset target property to null in this case
                    targetProperty = null;
                }
                else
                {
                    // Match namespace as well if specified
                    if (namespace == null ||
                        namespace.length() == 0 ||
                        namespace.equals("*") ||
                        curs.getName().getNamespaceURI().equals(namespace))
                    {
                        // Add it to the list
                        result.addToList(findAnnotation(curs));

                        // Set target property if target name is "*",
                        // Otherwise if target property does not match current, then
                        // set to null
                        if (targetProperty != null)
                        {
                            if (targetProperty.getLocalPart().equals("*"))
                            {
                                targetProperty = curs.getName();
                            }
                            else if (!targetProperty.getLocalPart().equals(curs.getName().getLocalPart()))
                            {
                                // Not a match, unset target property
                                targetProperty = null;
                            }
                        }
                    }
                }

                // Skip over child elements
                if (tt.isStart())
                {
                    tt = curs.toEndToken();
                }

                tt = curs.toNextToken();
            }
        }

        curs.dispose();

        // Set the targets for this XMLList.
        result.setTargets(this, targetProperty);

        return result;
    }

    /**
     *
     * @return
     */
    private XMLList matchDescendantAttributes(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();

        // Set the targets for this XMLList.
        result.setTargets(this, null);

        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            int nestLevel = 1;

            while (nestLevel > 0)
            {
                tt = curs.toNextToken();

                // Only try to match names for attributes
                if (tt.isAttr())
                {
                    if (qnameMatches(xmlName, curs.getName()))
                    {
                        result.addToList(findAnnotation(curs));
                    }
                }

                if (tt.isStart())
                {
                    nestLevel++;
                }
                else if (tt.isEnd())
                {
                    nestLevel--;
                }
                else if (tt.isEnddoc())
                {
                    // Shouldn't get here, but just in case.
                    break;
                }
            }
        }

        curs.dispose();

        return result;
    }

    /**
     *
     * @return
     */
    private XMLList matchDescendantChildren(XMLName xmlName)
    {
        XMLList result = new XMLList(lib);
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();

        // Set the targets for this XMLList.
        result.setTargets(this, null);

        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            int nestLevel = 1;

            while (nestLevel > 0)
            {
                tt = curs.toNextToken();

                if (!tt.isAttr() && !tt.isEnd() && !tt.isEnddoc())
                {
                    // Only try to match names for elements or processing instructions.
                    if (!tt.isStart() && !tt.isProcinst())
                    {
                        // Not an element or procinst, only add if qname is all
                        if (xmlName.localName().equals("*"))
                        {
                            result.addToList(findAnnotation(curs));
                        }
                    }
                    else
                    {
                        if (qnameMatches(xmlName, curs.getName()))
                        {
                            result.addToList(findAnnotation(curs));
                        }
                    }
                }

                if (tt.isStart())
                {
                    nestLevel++;
                }
                else if (tt.isEnd())
                {
                    nestLevel--;
                }
                else if (tt.isEnddoc())
                {
                    // Shouldn't get here, but just in case.
                    break;
                }
            }
        }

        curs.dispose();

        return result;
    }

    /**
     *
     * @param tokenType
     * @return
     */
    private XMLList matchChildren(XmlCursor.TokenType tokenType)
    {
        return matchChildren(tokenType, XMLName.formStar());
    }

    /**
     *
     * @return
     */
    private XMLList matchChildren(XmlCursor.TokenType tokenType, XMLName name)
    {
        XMLList result = new XMLList(lib);
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();
        javax.xml.namespace.QName qname = new javax.xml.namespace.QName(name.uri(), name.localName());
        javax.xml.namespace.QName targetProperty = qname;

        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            tt = curs.toFirstContentToken();

            while (!tt.isEnd())
            {
                if (tt == tokenType)
                {
                    // Only try to match names for elements or processing instructions.
                    if (!tt.isStart() && !tt.isProcinst())
                    {
                        // Not an element or no name specified.
                        result.addToList(findAnnotation(curs));

                        // Reset target property to null in this case
                        targetProperty = null;
                    }
                    else
                    {
                        // Match names as well
                        if (qnameMatches(name, curs.getName()))
                        {
                            // Add it to the list
                            result.addToList(findAnnotation(curs));

                            // Set target property if target name is "*",
                            // Otherwise if target property does not match current, then
                            // set to null
                            if (targetProperty != null)
                            {
                                if (targetProperty.getLocalPart().equals("*"))
                                {
                                    targetProperty = curs.getName();
                                }
                                else if (!targetProperty.getLocalPart().equals(curs.getName().getLocalPart()))
                                {
                                    // Not a match, unset target property
                                    targetProperty = null;
                                }
                            }
                        }
                    }
                }

                // Skip over child elements
                if (tt.isStart())
                {
                    tt = curs.toEndToken();
                }

                tt = curs.toNextToken();
            }
        }

        curs.dispose();

        if (tokenType == XmlCursor.TokenType.START)
        {
            // Set the targets for this XMLList.
            result.setTargets(this, targetProperty);
        }

        return result;

    }

    /**
     *
     * @param template
     * @param match
     * @return
     */
    private boolean qnameMatches(XMLName template, javax.xml.namespace.QName match)
    {
        boolean matches = false;

        if (template.uri() == null ||
            template.uri().equals(match.getNamespaceURI()))
        {
            // URI OK, test name
            if (template.localName().equals("*") ||
                template.localName().equals(match.getLocalPart()))
            {
                matches = true;
            }
        }

        return matches;
    }

    //
    //
    // Methods from section 12.4.4 in the spec
    //
    //

    /**
     * The addNamespace method adds a namespace declaration to the in scope
     * namespaces for this XML object and returns this XML object.
     *
     * @param toAdd
     */
    XML addNamespace(Namespace ns)
    {
        // When a namespace is used it will be added automatically
        // to the inScopeNamespaces set. There is no need to add
        // Namespaces with undefined prefixes.
        String nsPrefix = ns.prefix();
        if (nsPrefix == null) return this;

        XmlCursor cursor = newCursor();

        try
        {
            if(!cursor.isContainer()) return this;

            javax.xml.namespace.QName qname = cursor.getName();
            // Don't add a default namespace declarations to containers
            // with QNames in no namespace.
            if(qname.getNamespaceURI().equals("") &&
               nsPrefix.equals("")) return this;

            // Get all declared namespaces that are in scope
            Map prefixToURI = NamespaceHelper.getAllNamespaces(lib, cursor);

            String uri = (String)prefixToURI.get(nsPrefix);
            if(uri != null)
            {
                // Check if the Namespace is not already in scope
                if(uri.equals(ns.uri())) return this;

                cursor.push();

                // Let's see if we have to delete a namespace declaration
                while(cursor.toNextToken().isAnyAttr())
                {
                    if(cursor.isNamespace())
                    {
                        qname = cursor.getName();
                        String prefix = qname.getLocalPart();
                        if(prefix.equals(nsPrefix))
                        {
                            // Delete the current Namespace declaration
                            cursor.removeXml();
                            break;
                        }
                    }
                }

                cursor.pop();
            }

            cursor.toNextToken();
            cursor.insertNamespace(nsPrefix, ns.uri());
        }
        finally
        {
            cursor.dispose();
        }

        return this;
    }

    /**
     *
     * @param xml
     * @return
     */
    XML appendChild(Object xml)
    {
        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        // Move the cursor to the end of this element
        if (curs.isStart())
        {
            curs.toEndToken();
        }

        insertChild(curs, xml);

        curs.dispose();

        return this;
    }

    /**
     *
     * @param name
     * @return
     */
    XMLList attribute(XMLName xmlName)
    {
        return matchAttributes(xmlName);
    }

    /**
     *
     * @return
     */
    XMLList attributes()
    {
        XMLName xmlName = XMLName.formStar();
        return matchAttributes(xmlName);
    }

    XMLList child(long index)
    {
        XMLList result = new XMLList(lib);
        result.setTargets(this, null);
        result.addToList(getXmlChild(index));
        return result;
    }

    XMLList child(XMLName xmlName)
    {
        if (xmlName == null)
            return new XMLList(lib);

        XMLList result;
        if (xmlName.localName().equals("*"))
        {
            result = allChildNodes(xmlName.uri());
        }
        else
        {
            result = matchChildren(XmlCursor.TokenType.START, xmlName);
        }

        return result;
    }

    /**
     *
     * @param index
     * @return
     */
    XML getXmlChild(long index)
    {
        XML result = null;
        XmlCursor curs = newCursor();

        if (moveToChild(curs, index, false, true))
        {
            result = createXML(lib, curs);
        }

        curs.dispose();

        return result;
    }

    /**
     *
     * @return
     */
    int childIndex()
    {
        int index = 0;

        XmlCursor curs = newCursor();

        TokenType tt = curs.currentTokenType();
        while (true)
        {
            if (tt.isText())
            {
                index++;
                if (!curs.toPrevSibling())
                {
                    break;
                }
            }
            else if (tt.isStart())
            {
                tt = curs.toPrevToken();
                if (tt.isEnd())
                {
                    curs.toNextToken();
                    if (!curs.toPrevSibling())
                    {
                        break;
                    }

                    index++;
                }
                else
                {
                    // Hit the parent start tag so get out we're down counting children.
                    break;
                }
            }
            else if (tt.isComment() || tt.isProcinst())
            {
                curs.toPrevToken();
            }
            else
            {
                break;
            }

            tt = curs.currentTokenType();
        }

        index = curs.currentTokenType().isStartdoc() ? -1 : index;

        curs.dispose();

        return index;
    }

    /**
     *
     * @return
     */
    XMLList children()
    {
        return allChildNodes(null);
    }

    /**
     *
     * @return
     */
    XMLList comments()
    {
        return matchChildren(XmlCursor.TokenType.COMMENT);
    }

    /**
     *
     * @param xml
     * @return
     */
    boolean contains(Object xml)
    {
        boolean result = false;

        if (xml instanceof XML)
        {
            result = equivalentXml(xml);
        }

        return result;
    }

    /**
     *
     * @return
     */
    Object copy()
    {
        XmlCursor srcCurs = newCursor();

        if (srcCurs.isStartdoc())
        {
            srcCurs.toFirstContentToken();
        }

        XML xml = createEmptyXML(lib);

        XmlCursor destCurs = xml.newCursor();
        destCurs.toFirstContentToken();

        srcCurs.copyXml(destCurs);

        destCurs.dispose();
        srcCurs.dispose();

        return xml;
    }

    /**
     *
     * @param name
     * @return
     */
    XMLList descendants(XMLName xmlName)
    {
        XMLList result;
        if (xmlName.isAttributeName())
        {
            result = matchDescendantAttributes(xmlName);
        }
        else
        {
            result = matchDescendantChildren(xmlName);
        }

        return result;
    }

    /**
     * The inScopeNamespaces method returns an Array of Namespace objects
     * representing the namespaces in scope for this XML object in the
     * context of its parent.
     *
     * @return Array of all Namespaces in scope for this XML Object.
     */
    Object[] inScopeNamespaces()
    {
        XmlCursor cursor = newCursor();
        Object[] namespaces = NamespaceHelper.inScopeNamespaces(lib, cursor);
        cursor.dispose();
        return namespaces;
    }

    /**
     *
     * @param child
     * @param xml
     */
    XML insertChildAfter(Object child, Object xml)
    {
        if (child == null)
        {
            // Spec says inserting after nothing is the same as prepending
            prependChild(xml);
        }
        else if (child instanceof XML)
        {
            insertChild((XML) child, xml, APPEND_CHILD);
        }

        return this;
    }

    /**
     *
     * @param child
     * @param xml
     */
    XML insertChildBefore(Object child, Object xml)
    {
        if (child == null)
        {
            // Spec says inserting before nothing is the same as appending
            appendChild(xml);
        }
        else if (child instanceof XML)
        {
            insertChild((XML) child, xml, PREPEND_CHILD);
        }

        return this;
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
        return !hasSimpleContent();
    }

    /**
     *
     * @return
     */
    boolean hasSimpleContent()
    {
        boolean simpleContent = false;

        XmlCursor curs = newCursor();

        if (curs.isAttr() || curs.isText()) {
            return true;
        }

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        simpleContent = !(curs.toFirstChild());

        curs.dispose();

        return simpleContent;
    }

    /**
     * Length of an XML object is always 1, it's a list of XML objects of size 1.
     *
     * @return
     */
    int length()
    {
        return 1;
    }

    /**
     *
     * @return
     */
    String localName()
    {
        XmlCursor cursor = newCursor();
        if (cursor.isStartdoc())
            cursor.toFirstContentToken();

        String name = null;

        if(cursor.isStart() ||
           cursor.isAttr() ||
           cursor.isProcinst())
        {
            javax.xml.namespace.QName qname = cursor.getName();
            name = qname.getLocalPart();
        }
        cursor.dispose();

        return name;
    }

    /**
     * The name method returns the qualified name associated with this XML object.
     *
     * @return The qualified name associated with this XML object.
     */
    QName name()
    {
        XmlCursor cursor = newCursor();
        if (cursor.isStartdoc())
            cursor.toFirstContentToken();

        QName name = null;

        if(cursor.isStart() ||
           cursor.isAttr() ||
           cursor.isProcinst())
        {
            javax.xml.namespace.QName qname = cursor.getName();
            if(cursor.isProcinst())
            {
                name = new QName(lib, "", qname.getLocalPart(), "");
            }
            else
            {
                String uri = qname.getNamespaceURI();
                String prefix = qname.getPrefix();
                name = new QName(lib, uri, qname.getLocalPart(), prefix);
            }
        }

        cursor.dispose();

        return name;
    }

    /**
     *
     * @param prefix
     * @return
     */
    Object namespace(String prefix)
    {
        XmlCursor cursor = newCursor();
        if (cursor.isStartdoc())
        {
            cursor.toFirstContentToken();
        }

        Object result = null;

        if (prefix == null)
        {
            if(cursor.isStart() ||
               cursor.isAttr())
            {
                Object[] inScopeNS = NamespaceHelper.inScopeNamespaces(lib, cursor);
                // XXX Is it reaaly necessary to create the second cursor?
                XmlCursor cursor2 = newCursor();
                if (cursor2.isStartdoc())
                    cursor2.toFirstContentToken();

                result = NamespaceHelper.getNamespace(lib, cursor2, inScopeNS);

                cursor2.dispose();
            }
        }
        else
        {
            Map prefixToURI = NamespaceHelper.getAllNamespaces(lib, cursor);
            String uri = (String)prefixToURI.get(prefix);
            result = (uri == null) ? Undefined.instance : new Namespace(lib, prefix, uri);
        }

        cursor.dispose();

        return result;
    }

    /**
     *
     * @return
     */
    Object[] namespaceDeclarations()
    {
        XmlCursor cursor = newCursor();
        Object[] namespaces = NamespaceHelper.namespaceDeclarations(lib, cursor);
        cursor.dispose();
        return namespaces;
    }

    /**
     *
     * @return
     */
    Object nodeKind()
    {
        String result;
        XmlCursor.TokenType tt = tokenType();

        if (tt == XmlCursor.TokenType.ATTR)
        {
            result = "attribute";
        }
        else if (tt == XmlCursor.TokenType.TEXT)
        {
            result = "text";
        }
        else if (tt == XmlCursor.TokenType.COMMENT)
        {
            result = "comment";
        }
        else if (tt == XmlCursor.TokenType.PROCINST)
        {
            result = "processing-instruction";
        }
        else if (tt == XmlCursor.TokenType.START)
        {
            result = "element";
        }
        else
        {
            // A non-existant node has the nodeKind() of text
            result = "text";
        }

        return result;
    }

    /**
     *
     */
    void normalize()
    {
        XmlCursor curs = newCursor();
        TokenType tt = curs.currentTokenType();

        // Walk through the tokens removing empty text nodes and merging adjacent text nodes.
        if (tt.isStartdoc())
        {
            tt = curs.toFirstContentToken();
        }

        if (tt.isContainer())
        {
            int nestLevel = 1;
            String previousText = null;

            while (nestLevel > 0)
            {
                tt = curs.toNextToken();

                if (tt == XmlCursor.TokenType.TEXT)
                {
                    String currentText = curs.getChars().trim();

                    if (currentText.trim().length() == 0)
                    {
                        // Empty text node, remove.
                        removeToken(curs);
                        curs.toPrevToken();
                    }
                    else if (previousText == null)
                    {
                        // No previous text node, reset to trimmed version
                        previousText = currentText;
                    }
                    else
                    {
                        // It appears that this case never happens with XBeans.
                        // Previous text node exists, concatenate
                        String newText = previousText + currentText;

                        curs.toPrevToken();
                        removeToken(curs);
                        removeToken(curs);
                        curs.insertChars(newText);
                    }
                }
                else
                {
                    previousText = null;
                }

                if (tt.isStart())
                {
                    nestLevel++;
                }
                else if (tt.isEnd())
                {
                    nestLevel--;
                }
                else if (tt.isEnddoc())
                {
                    // Shouldn't get here, but just in case.
                    break;
                }
            }
        }


        curs.dispose();
    }

    /**
     *
     * @return
     */
    Object parent()
    {
        Object parent;

        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            // At doc level - no parent
            parent = Undefined.instance;
        }
        else
        {
            if (curs.toParent())
            {
                if (curs.isStartdoc())
                {
                    // Was top-level - no parent
                    parent = Undefined.instance;
                }
                else
                {
                    parent = getFromAnnotation(lib, findAnnotation(curs));
                }
            }
            else
            {
                // No parent
                parent = Undefined.instance;
            }
        }

        curs.dispose();

        return parent;
    }

    /**
     *
     * @param xml
     * @return
     */
    XML prependChild (Object xml)
    {
        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        // Move the cursor to the first content token
        curs.toFirstContentToken();

        insertChild(curs, xml);

        curs.dispose();

        return this;
    }

    /**
     *
     * @return
     */
    Object processingInstructions(XMLName xmlName)
    {
        return matchChildren(XmlCursor.TokenType.PROCINST, xmlName);
    }

    /**
     *
     * @param name
     * @return
     */
    boolean propertyIsEnumerable(Object name)
    {
        boolean result;
        if (name instanceof Integer) {
            result = (((Integer)name).intValue() == 0);
        } else if (name instanceof Number) {
            double x = ((Number)name).doubleValue();
            // Check that number is posotive 0
            result = (x == 0.0 && 1.0 / x > 0);
        } else {
            result = ScriptRuntime.toString(name).equals("0");
        }
        return result;
    }

    /**
     *
     * @param namespace
     */
    XML removeNamespace(Namespace ns)
    {
        XmlCursor cursor = newCursor();

        try
        {
            if(cursor.isStartdoc())
                cursor.toFirstContentToken();
            if(!cursor.isStart()) return this;

            String nsPrefix = ns.prefix();
            String nsURI = ns.uri();
            Map prefixToURI = new HashMap();
            int depth = 1;

            while(!(cursor.isEnd() && depth == 0))
            {
                if(cursor.isStart())
                {
                    // Get the namespaces declared in this element.
                    // The ones with undefined prefixes are not candidates
                    // for removal because they are used.
                    prefixToURI.clear();
                    NamespaceHelper.getNamespaces(cursor, prefixToURI);
                    ObjArray inScopeNSBag = new ObjArray();
                    Iterator i = prefixToURI.entrySet().iterator();
                    while(i.hasNext())
                    {
                        Map.Entry entry = (Map.Entry)i.next();
                        ns = new Namespace(lib, (String)entry.getKey(), (String)entry.getValue());
                        inScopeNSBag.add(ns);
                    }

                    // Add the URI we are looking for to avoid matching
                    // non-existing Namespaces.
                    ns = new Namespace(lib, nsURI);
                    inScopeNSBag.add(ns);

                    Object[] inScopeNS = inScopeNSBag.toArray();

                    // Check the element name
                    Namespace n = NamespaceHelper.getNamespace(lib, cursor,
                                                               inScopeNS);
                    if(nsURI.equals(n.uri()) &&
                       (nsPrefix == null ||
                        nsPrefix.equals(n.prefix())))
                    {
                        // This namespace is used
                        return this;
                    }

                    // Check the attributes
                    cursor.push();
                    boolean hasNext = cursor.toFirstAttribute();
                    while(hasNext)
                    {
                        n = NamespaceHelper.getNamespace(lib, cursor, inScopeNS);
                        if(nsURI.equals(n.uri()) &&
                           (nsPrefix == null ||
                            nsPrefix.equals(n.prefix())))
                        {
                            // This namespace is used
                            return this;
                        }

                        hasNext = cursor.toNextAttribute();
                    }
                    cursor.pop();

                    if(nsPrefix == null)
                    {
                        // Remove all namespaces declarations that match nsURI
                        i = prefixToURI.entrySet().iterator();
                        while(i.hasNext())
                        {
                            Map.Entry entry = (Map.Entry)i.next();
                            if(entry.getValue().equals(nsURI))
                                NamespaceHelper.removeNamespace(cursor, (String)entry.getKey());
                        }
                    }
                    else if(nsURI.equals(prefixToURI.get(nsPrefix)))
                    {
                        // Remove the namespace declaration that matches nsPrefix
                        NamespaceHelper.removeNamespace(cursor, String.valueOf(nsPrefix));
                    }
                }

                switch(cursor.toNextToken().intValue())
                {
                case XmlCursor.TokenType.INT_START:
                    depth++;
                    break;
                case XmlCursor.TokenType.INT_END:
                    depth--;
                    break;
                }
            }
        }
        finally
        {
            cursor.dispose();
        }

        return this;
    }

    XML replace(long index, Object xml)
    {
        XMLList xlChildToReplace = (XMLList)child(index);
        if (xlChildToReplace.length() > 0)
        {
            // One exists an that index
            XML childToReplace = (XML)xlChildToReplace.item(0);
            insertChildAfter(childToReplace, xml);
            removeChild(index);
        }
        return this;
    }

    /**
     *
     * @param propertyName
     * @param xml
     * @return
     */
    XML replace(XMLName xmlName, Object xml)
    {
        putXMLProperty(xmlName, xml);
        return this;
    }

    /**
     *
     * @param xml
     */
    XML setChildren(Object xml)
    {
        // remove all children
        XMLName xmlName = XMLName.formStar();
        XMLList matches = (XMLList)getPropertyList(xmlName);
        matches.remove();

        // append new children
        appendChild(xml);

        return this;
    }

    /**
     *
     * @param name
     */
    void setLocalName(String localName)
    {
        XmlCursor cursor = newCursor();

        try
        {
            if(cursor.isStartdoc())
                cursor.toFirstContentToken();

            if(cursor.isText() || cursor.isComment()) return;


            javax.xml.namespace.QName qname = cursor.getName();
            cursor.setName(new javax.xml.namespace.QName(
                qname.getNamespaceURI(), localName, qname.getPrefix()));
        }
        finally
        {
            cursor.dispose();
        }
    }

    /**
     *
     * @param name
     */
    void setName(QName qname)
    {
        XmlCursor cursor = newCursor();

        try
        {
            if(cursor.isStartdoc())
                cursor.toFirstContentToken();

            if(cursor.isText() || cursor.isComment()) return;

            if(cursor.isProcinst())
            {
                String localName = qname.localName();
                cursor.setName(new javax.xml.namespace.QName(localName));
            }
            else
            {
                String prefix = qname.prefix();
                if (prefix == null) { prefix = ""; }
                cursor.setName(new javax.xml.namespace.QName(
                    qname.uri(), qname.localName(), prefix));
            }
        }
        finally
        {
            cursor.dispose();
        }
    }

    /**
     *
     * @param ns
     */
    void setNamespace(Namespace ns)
    {
        XmlCursor cursor = newCursor();

        try
        {
            if(cursor.isStartdoc())
                cursor.toFirstContentToken();

            if(cursor.isText() ||
               cursor.isComment() ||
               cursor.isProcinst()) return;

            String prefix = ns.prefix();
            if (prefix == null) {
                prefix = "";
            }
            cursor.setName(new javax.xml.namespace.QName(
                ns.uri(), localName(), prefix));
        }
        finally
        {
            cursor.dispose();
        }
    }

    /**
     *
     * @return
     */
    XMLList text()
    {
        return matchChildren(XmlCursor.TokenType.TEXT);
    }

    /**
     *
     * @return
     */
    public String toString()
    {
        String result;
        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        if (curs.isText())
        {
             result = curs.getChars();
        }
        else if (curs.isStart() && hasSimpleContent())
        {
            result = curs.getTextValue();
        }
        else
        {
            result = toXMLString(0);
        }

        return result;
    }

    String toSource(int indent)
    {
        // XXX Does toXMLString always return valid XML literal?
        return toXMLString(indent);
    }

    /**
     *
     * @return
     */
    String toXMLString(int indent)
    {
        // XXX indent is ignored

        String result;

        XmlCursor curs = newCursor();

        if (curs.isStartdoc())
        {
            curs.toFirstContentToken();
        }

        try
        {
            if (curs.isText())
            {
                result = curs.getChars();
            }
            else if (curs.isAttr())
            {
                result = curs.getTextValue();
            }
            else if (curs.isComment() || curs.isProcinst())
            {
                result = XML.dumpNode(curs, getOptions());

                // todo: XBeans-dependent hack here
                // If it's a comment or PI, take off the xml-frament stuff
                String start = "<xml-fragment>";
                String end = "</xml-fragment>";

                if (result.startsWith(start))
                {
                    result = result.substring(start.length());
                }

                if (result.endsWith(end))
                {
                    result = result.substring(0, result.length() - end.length());
                }
            }
            else
            {
                result = XML.dumpNode(curs, getOptions());
            }
        }
        finally
        {
            curs.dispose();
        }

        return result;
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

        if (target instanceof XML)
        {
            XML otherXml = (XML) target;

            // Compare with toString() if either side is text node or attribute
            // otherwise compare as XML
            XmlCursor.TokenType thisTT = tokenType();
            XmlCursor.TokenType otherTT = otherXml.tokenType();
            if (thisTT == XmlCursor.TokenType.ATTR || otherTT == XmlCursor.TokenType.ATTR ||
                thisTT == XmlCursor.TokenType.TEXT || otherTT == XmlCursor.TokenType.TEXT)
            {
                result = toString().equals(otherXml.toString());
            }
            else
            {
                XmlCursor cursOne = newCursor();
                XmlCursor cursTwo = otherXml.newCursor();

                result = LogicalEquality.nodesEqual(cursOne, cursTwo);

                cursOne.dispose();
                cursTwo.dispose();

// Old way of comparing by string.
//                boolean orgPrettyPrinting = prototype.prettyPrinting;
//                prototype.prettyPrinting = true;
//                result = toXMLString(0).equals(otherXml.toXMLString(0));
//                prototype.prettyPrinting = orgPrettyPrinting;
            }
        }
        else if (target instanceof XMLList)
        {
            XMLList otherList = (XMLList) target;

            if (otherList.length() == 1)
            {
                result = equivalentXml(otherList.getXmlFromAnnotation(0));
            }
        }
        else if (hasSimpleContent())
        {
            String otherStr = ScriptRuntime.toString(target);

            result = toString().equals(otherStr);
        }

        return result;
    }

    /**
     *
     * @param name
     * @param start
     * @return
     */
    XMLList getPropertyList(XMLName name)
    {
        XMLList result;

        // Get the named property
        if (name.isDescendants())
        {
            result = descendants(name);
        }
        else if (name.isAttributeName())
        {
            result = attribute(name);
        }
        else
        {
            result = child(name);
        }

        return result;
    }

    protected Object jsConstructor(Context cx, boolean inNewExpr,
                                   Object[] args)
    {
        if (args.length == 0) {
            return createEmptyXML(lib);
        } else {
            Object arg0 = args[0];
            if (!inNewExpr && arg0 instanceof XML) {
                // XML(XML) returns the same object.
                return arg0;
            }
            return createFromJS(lib, arg0);
        }
    }

    /**
     * See ECMA 357, 11_2_2_1, Semantics, 3_f.
     */
    public Scriptable getExtraMethodSource(Context cx)
    {
        if (hasSimpleContent()) {
            String src = toString();
            return ScriptRuntime.toObjectOrNull(cx, src);
        }
        return null;
    }

    //
    // Implementation of Wrapper
    //

    public Object unwrap()
    {
        XmlObject xo;
        XmlCursor cursor = newCursor();
        try {
            xo = cursor.getObject();
        } finally {
            cursor.dispose();
        }
        return xo;
    }

}
