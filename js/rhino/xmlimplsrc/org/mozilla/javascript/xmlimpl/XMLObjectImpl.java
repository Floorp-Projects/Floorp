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
 * Igor Bukanov
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

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

/**
 *  This abstract class describes what all XML objects (XML, XMLList) should have in common.
 *
 * @see XML
 */
abstract class XMLObjectImpl extends XMLObject
{
    private static final Object XMLOBJECT_TAG = new Object();

    protected final XMLLibImpl lib;
    protected boolean prototypeFlag;

    protected XMLObjectImpl(XMLLibImpl lib)
    {
        this.lib = lib;
    }

    /**
     * ecmaHas(cx, id) calls this after resolving when id to XMLName
     * and checking it is not Uint32 index.
     */
    abstract boolean hasXMLProperty(XMLName name);

    /**
     * ecmaGet(cx, id) calls this after resolving when id to XMLName
     * and checking it is not Uint32 index.
     */
    abstract Object getXMLProperty(XMLName name);

    /**
     * ecmaPut(cx, id, value) calls this after resolving when id to XMLName
     * and checking it is not Uint32 index.
     */
    abstract void putXMLProperty(XMLName name, Object value);

    /**
     * ecmaDelete(cx, id) calls this after resolving when id to XMLName
     * and checking it is not Uint32 index.
     */
    abstract void deleteXMLProperty(XMLName name);

    /**
     * Test XML equality with target the target.
     */
    abstract boolean equivalentXml(Object target);

    // Methods from section 12.4.4 in the spec
    abstract XML addNamespace(Namespace ns);
    abstract XML appendChild(Object xml);
    abstract XMLList attribute(XMLName xmlName);
    abstract XMLList attributes();
    abstract XMLList child(long index);
    abstract XMLList child(XMLName xmlName);
    abstract int childIndex();
    abstract XMLList children();
    abstract XMLList comments();
    abstract boolean contains(Object xml);
    abstract Object copy();
    abstract XMLList descendants(XMLName xmlName);
    abstract Object[] inScopeNamespaces();
    abstract XML insertChildAfter(Object child, Object xml);
    abstract XML insertChildBefore(Object child, Object xml);
    abstract boolean hasOwnProperty(XMLName xmlName);
    abstract boolean hasComplexContent();
    abstract boolean hasSimpleContent();
    abstract int length();
    abstract String localName();
    abstract QName name();
    abstract Object namespace(String prefix);
    abstract Object[] namespaceDeclarations();
    abstract Object nodeKind();
    abstract void normalize();
    abstract Object parent();
    abstract XML prependChild(Object xml);
    abstract Object processingInstructions(XMLName xmlName);
    abstract boolean propertyIsEnumerable(XMLName xmlName);
    abstract XML removeNamespace(Namespace ns);
    abstract XML replace(long index, Object xml);
    abstract XML replace(XMLName name, Object xml);
    abstract XML setChildren(Object xml);
    abstract void setLocalName(String name);
    abstract void setName(QName xmlName);
    abstract void setNamespace(Namespace ns);
    abstract XMLList text();
    public abstract String toString();
    abstract String toXMLString();
    abstract Object valueOf();

    protected abstract Object jsConstructor(Context cx, boolean inNewExpr,
                                            Object[] args);


    final Object getMethod(String id)
    {
        return super.get(id, this);
    }

    //
    //
    // Methods overriding ScriptableObject
    //
    //

    public final Object getDefaultValue(Class hint)
    {
        return toString();
    }

    protected final Scriptable defaultParentScope()
    {
        return lib.globalScope();
    }

    public void delete(String name)
    {
        throw new IllegalArgumentException("String: [" + name + "]");
    }

    /**
     * XMLObject always compare with any value and equivalentValues
     * never returns null for them but rather calls equivalentXml(value)
     * and box the result as Boolean.
     */
    public final Boolean equivalentValues(Object value)
    {
        boolean result = equivalentXml(value);

        return result ? Boolean.TRUE : Boolean.FALSE;
    }

    //
    //
    // Methods overriding XMLObject
    //
    //

    public final XMLLib lib()
    {
        return lib;
    }

    /**
     * Implementation of ECMAScript [[Has]]
     */
    public final boolean ecmaHas(Context cx, Object id)
    {
        if (cx == null) cx = Context.getCurrentContext();
        XMLName xmlName = lib.toXMLNameOrIndex(cx, id);
        if (xmlName == null) {
            long index = ScriptRuntime.lastUint32Result(cx);
            // XXX Fix this cast
            return has((int)index, this);
        }
        return hasXMLProperty(xmlName);
    }

    /**
     * Implementation of ECMAScript [[Get]]
     */
    public final Object ecmaGet(Context cx, Object id)
    {
        if (cx == null) cx = Context.getCurrentContext();
        XMLName xmlName = lib.toXMLNameOrIndex(cx, id);
        if (xmlName == null) {
            long index = ScriptRuntime.lastUint32Result(cx);
            // XXX Fix this cast
            Object result = get((int)index, this);
            if (result == Scriptable.NOT_FOUND) {
                result = Undefined.instance;
            }
            return result;
        }
        return getXMLProperty(xmlName);
    }

    /**
     * Implementation of ECMAScript [[Put]]
     */
    public final void ecmaPut(Context cx, Object id, Object value)
    {
        if (cx == null) cx = Context.getCurrentContext();
        XMLName xmlName = lib.toXMLNameOrIndex(cx, id);
        if (xmlName == null) {
            long index = ScriptRuntime.lastUint32Result(cx);
            // XXX Fix this cast
            put((int)index, this, value);
            return;
        }
        putXMLProperty(xmlName, value);
    }

    /**
     * Implementation of ECMAScript [[Delete]]
     */
    public final boolean ecmaDelete(Context cx, Object id)
    {
        if (cx == null) cx = Context.getCurrentContext();
        XMLName xmlName = lib.toXMLNameOrIndex(cx, id);
        if (xmlName == null) {
            long index = ScriptRuntime.lastUint32Result(cx);
            // XXX Fix this
            delete((int)index);
            return true;
        }
        deleteXMLProperty(xmlName);
        return true;
    }

    //
    //
    // IdScriptableObject machinery
    //
    //

    final void exportAsJSClass(boolean sealed)
    {
        prototypeFlag = true;
        exportAsJSClass(MAX_PROTOTYPE_ID, lib.globalScope(), sealed);
    }

// #string_id_map#
    private final static int
        Id_constructor             = 1,

        Id_addNamespace            = 2,
        Id_appendChild             = 3,
        Id_attribute               = 4,
        Id_attributes              = 5,
        Id_child                   = 6,
        Id_childIndex              = 7,
        Id_children                = 8,
        Id_comments                = 9,
        Id_contains                = 10,
        Id_copy                    = 11,
        Id_descendants             = 12,
        Id_inScopeNamespaces       = 13,
        Id_insertChildAfter        = 14,
        Id_insertChildBefore       = 15,
        Id_hasOwnProperty          = 16,
        Id_hasComplexContent       = 17,
        Id_hasSimpleContent        = 18,
        Id_length                  = 19,
        Id_localName               = 20,
        Id_name                    = 21,
        Id_namespace               = 22,
        Id_namespaceDeclarations   = 23,
        Id_nodeKind                = 24,
        Id_normalize               = 25,
        Id_parent                  = 26,
        Id_prependChild            = 27,
        Id_processingInstructions  = 28,
        Id_propertyIsEnumerable    = 29,
        Id_removeNamespace         = 30,
        Id_replace                 = 31,
        Id_setChildren             = 32,
        Id_setLocalName            = 33,
        Id_setName                 = 34,
        Id_setNamespace            = 35,
        Id_text                    = 36,
        Id_toString                = 37,
        Id_toXMLString             = 38,
        Id_valueOf                 = 39,

        MAX_PROTOTYPE_ID           = 39;

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-07-19 13:13:35 CEST
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 4: c=s.charAt(0);
                if (c=='c') { X="copy";id=Id_copy; }
                else if (c=='n') { X="name";id=Id_name; }
                else if (c=='t') { X="text";id=Id_text; }
                break L;
            case 5: X="child";id=Id_child; break L;
            case 6: c=s.charAt(0);
                if (c=='l') { X="length";id=Id_length; }
                else if (c=='p') { X="parent";id=Id_parent; }
                break L;
            case 7: c=s.charAt(0);
                if (c=='r') { X="replace";id=Id_replace; }
                else if (c=='s') { X="setName";id=Id_setName; }
                else if (c=='v') { X="valueOf";id=Id_valueOf; }
                break L;
            case 8: switch (s.charAt(2)) {
                case 'S': X="toString";id=Id_toString; break L;
                case 'd': X="nodeKind";id=Id_nodeKind; break L;
                case 'i': X="children";id=Id_children; break L;
                case 'm': X="comments";id=Id_comments; break L;
                case 'n': X="contains";id=Id_contains; break L;
                } break L;
            case 9: switch (s.charAt(2)) {
                case 'c': X="localName";id=Id_localName; break L;
                case 'm': X="namespace";id=Id_namespace; break L;
                case 'r': X="normalize";id=Id_normalize; break L;
                case 't': X="attribute";id=Id_attribute; break L;
                } break L;
            case 10: c=s.charAt(0);
                if (c=='a') { X="attributes";id=Id_attributes; }
                else if (c=='c') { X="childIndex";id=Id_childIndex; }
                break L;
            case 11: switch (s.charAt(0)) {
                case 'a': X="appendChild";id=Id_appendChild; break L;
                case 'c': X="constructor";id=Id_constructor; break L;
                case 'd': X="descendants";id=Id_descendants; break L;
                case 's': X="setChildren";id=Id_setChildren; break L;
                case 't': X="toXMLString";id=Id_toXMLString; break L;
                } break L;
            case 12: c=s.charAt(0);
                if (c=='a') { X="addNamespace";id=Id_addNamespace; }
                else if (c=='p') { X="prependChild";id=Id_prependChild; }
                else if (c=='s') {
                    c=s.charAt(3);
                    if (c=='L') { X="setLocalName";id=Id_setLocalName; }
                    else if (c=='N') { X="setNamespace";id=Id_setNamespace; }
                }
                break L;
            case 14: X="hasOwnProperty";id=Id_hasOwnProperty; break L;
            case 15: X="removeNamespace";id=Id_removeNamespace; break L;
            case 16: c=s.charAt(0);
                if (c=='h') { X="hasSimpleContent";id=Id_hasSimpleContent; }
                else if (c=='i') { X="insertChildAfter";id=Id_insertChildAfter; }
                break L;
            case 17: c=s.charAt(3);
                if (c=='C') { X="hasComplexContent";id=Id_hasComplexContent; }
                else if (c=='c') { X="inScopeNamespaces";id=Id_inScopeNamespaces; }
                else if (c=='e') { X="insertChildBefore";id=Id_insertChildBefore; }
                break L;
            case 20: X="propertyIsEnumerable";id=Id_propertyIsEnumerable; break L;
            case 21: X="namespaceDeclarations";id=Id_namespaceDeclarations; break L;
            case 22: X="processingInstructions";id=Id_processingInstructions; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }
// #/string_id_map#

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor: {
            IdFunctionObject ctor;
            if (this instanceof XML) {
                ctor = new XMLCtor((XML)this, XMLOBJECT_TAG, id, 1);
            } else {
                ctor = new IdFunctionObject(this, XMLOBJECT_TAG, id, 1);
            }
            initPrototypeConstructor(ctor);
            return;
          }

          case Id_addNamespace:      arity=1; s="addNamespace";      break;
          case Id_appendChild:       arity=1; s="appendChild";       break;
          case Id_attribute:         arity=1; s="attribute";         break;
          case Id_attributes:        arity=0; s="attributes";        break;
          case Id_child:             arity=1; s="child";             break;
          case Id_childIndex:        arity=0; s="childIndex";        break;
          case Id_children:          arity=0; s="children";          break;
          case Id_comments:          arity=0; s="comments";          break;
          case Id_contains:          arity=1; s="contains";          break;
          case Id_copy:              arity=0; s="copy";              break;
          case Id_descendants:       arity=1; s="descendants";       break;
          case Id_hasComplexContent: arity=0; s="hasComplexContent"; break;
          case Id_hasOwnProperty:    arity=1; s="hasOwnProperty";    break;
          case Id_hasSimpleContent:  arity=0; s="hasSimpleContent";  break;
          case Id_inScopeNamespaces: arity=0; s="inScopeNamespaces"; break;
          case Id_insertChildAfter:  arity=2; s="insertChildAfter";  break;
          case Id_insertChildBefore: arity=2; s="insertChildBefore"; break;
          case Id_length:            arity=0; s="length";            break;
          case Id_localName:         arity=0; s="localName";         break;
          case Id_name:              arity=0; s="name";              break;
          case Id_namespace:         arity=1; s="namespace";         break;
          case Id_namespaceDeclarations:
            arity=0; s="namespaceDeclarations"; break;
          case Id_nodeKind:          arity=0; s="nodeKind";          break;
          case Id_normalize:         arity=0; s="normalize";         break;
          case Id_parent:            arity=0; s="parent";            break;
          case Id_prependChild:      arity=1; s="prependChild";      break;
          case Id_processingInstructions:
            arity=1; s="processingInstructions"; break;
          case Id_propertyIsEnumerable:
            arity=1; s="propertyIsEnumerable"; break;
          case Id_removeNamespace:   arity=1; s="removeNamespace";   break;
          case Id_replace:           arity=2; s="replace";           break;
          case Id_setChildren:       arity=1; s="setChildren";       break;
          case Id_setLocalName:      arity=1; s="setLocalName";      break;
          case Id_setName:           arity=1; s="setName";           break;
          case Id_setNamespace:      arity=1; s="setNamespace";      break;
          case Id_text:              arity=0; s="text";              break;
          case Id_toString:          arity=0; s="toString";          break;
          case Id_toXMLString:       arity=0; s="toXMLString";       break;
          case Id_valueOf:           arity=0; s="valueOf";           break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(XMLOBJECT_TAG, id, s, arity);
    }

    /**
     *
     * @param f
     * @param cx
     * @param scope
     * @param thisObj
     * @param args
     * @return
     * @throws JavaScriptException
     */
    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        if (!f.hasTag(XMLOBJECT_TAG)) {
            return super.execIdCall(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case Id_constructor: {
            return jsConstructor(cx, thisObj == null, args);
          }
          case Id_addNamespace: {
            Namespace ns = lib.castToNamespace(cx, arg(args, 0));
            return realThis(thisObj, f).addNamespace(ns);
          }
          case Id_appendChild:
            return realThis(thisObj, f).appendChild(arg(args, 0));
          case Id_attribute: {
            Object arg0 = arg(args, 0);
            if (arg0 == null || arg0 == Undefined.instance) {
                // XXX: E4X requires to throw the exception in this case
                // (whoch toAttributeName will) but the test suite assumes
                // it should be OK. Trust test suite for now.
                arg0 = ScriptRuntime.toString(arg0);
            }
            XMLName xmlName = lib.toAttributeNameImpl(cx, arg0);
            return realThis(thisObj, f).attribute(xmlName);
          }
          case Id_attributes:
            return realThis(thisObj, f).attributes();
          case Id_child: {
            XMLName xmlName = getXmlNameOrIndex(cx, args, 0);
            if (xmlName == null) {
                long index = ScriptRuntime.lastUint32Result(cx);
                return realThis(thisObj, f).child(index);
            } else {
                return realThis(thisObj, f).child(xmlName);
            }
          }
          case Id_childIndex:
            return new Integer(realThis(thisObj, f).childIndex());
          case Id_children:
            return realThis(thisObj, f).children();
          case Id_comments:
            return realThis(thisObj, f).comments();
          case Id_contains:
            return wrap_boolean(realThis(thisObj, f).contains(arg(args, 0)));
          case Id_copy:
            return realThis(thisObj, f).copy();
          case Id_descendants: {
            XMLName xmlName = (args.length == 0)
                              ? XMLName.formStar()
                              : lib.toXMLName(cx, args[0]);
            return realThis(thisObj, f).descendants(xmlName);
          }
          case Id_inScopeNamespaces: {
            Object[] array = realThis(thisObj, f).inScopeNamespaces();
            return cx.newArray(scope, array);
          }
          case Id_insertChildAfter:
            return realThis(thisObj, f).insertChildAfter(arg(args, 0), arg(args, 1));
          case Id_insertChildBefore:
            return realThis(thisObj, f).insertChildBefore(arg(args, 0), arg(args, 1));
          case Id_hasOwnProperty: {
            XMLName xmlName = lib.toXMLName(cx, arg(args, 0));
            return wrap_boolean(
                realThis(thisObj, f).hasOwnProperty(xmlName));
          }
          case Id_hasComplexContent:
            return wrap_boolean(realThis(thisObj, f).hasComplexContent());
          case Id_hasSimpleContent:
            return wrap_boolean(realThis(thisObj, f).hasSimpleContent());
          case Id_length:
            return new Integer(realThis(thisObj, f).length());
          case Id_localName:
            return realThis(thisObj, f).localName();
          case Id_name:
            return realThis(thisObj, f).name();
          case Id_namespace: {
            String prefix = (args.length > 0)
                            ? ScriptRuntime.toString(args[0]) : null;
            return realThis(thisObj, f).namespace(prefix);
          }
          case Id_namespaceDeclarations: {
            Object[] array = realThis(thisObj, f).namespaceDeclarations();
            return cx.newArray(scope, array);
          }
          case Id_nodeKind:
            return realThis(thisObj, f).nodeKind();
          case Id_normalize:
            realThis(thisObj, f).normalize();
            return Undefined.instance;
          case Id_parent:
            return realThis(thisObj, f).parent();
          case Id_prependChild:
            return realThis(thisObj, f).prependChild(arg(args, 0));
          case Id_processingInstructions: {
            XMLName xmlName = (args.length > 0)
                              ? lib.toXMLName(cx, args[0])
                              : XMLName.formStar();
            return realThis(thisObj, f).processingInstructions(xmlName);
          }
          case Id_propertyIsEnumerable: {
            XMLName xmlName = lib.toXMLName(cx, arg(args, 0));
            return wrap_boolean(
                realThis(thisObj, f).propertyIsEnumerable(xmlName));
          }
          case Id_removeNamespace: {
            Namespace ns = lib.castToNamespace(cx, arg(args, 0));
            return realThis(thisObj, f).removeNamespace(ns);
          }
          case Id_replace: {
            XMLName xmlName = getXmlNameOrIndex(cx, args, 0);
            Object arg1 = arg(args, 1);
            if (xmlName == null) {
                long index = ScriptRuntime.lastUint32Result(cx);
                return realThis(thisObj, f).replace(index, arg1);
            } else {
                return realThis(thisObj, f).replace(xmlName, arg1);
            }
          }
          case Id_setChildren:
            return realThis(thisObj, f).setChildren(arg(args, 0));
          case Id_setLocalName: {
            String localName;
            Object arg = arg(args, 0);
            if (arg instanceof QName) {
                localName = ((QName)arg).localName();
            } else {
                localName = ScriptRuntime.toString(arg);
            }
            realThis(thisObj, f).setLocalName(localName);
            return Undefined.instance;
          }
          case Id_setName: {
            Object arg = (args.length != 0) ? args[0] : Undefined.instance;
            QName qname;
            if (arg instanceof QName) {
                qname = (QName)arg;
                if (qname.uri() == null) {
                    qname = lib.constructQNameFromString(cx, qname.localName());
                } else {
                    // E4X 13.4.4.35 requires to always construct QName
                    qname = lib.constructQName(cx, qname);
                }
            } else {
                qname = lib.constructQName(cx, arg);
            }
            realThis(thisObj, f).setName(qname);
            return Undefined.instance;
          }
          case Id_setNamespace: {
            Namespace ns = lib.castToNamespace(cx, arg(args, 0));
            realThis(thisObj, f).setNamespace(ns);
            return Undefined.instance;
          }
          case Id_text:
            return realThis(thisObj, f).text();
          case Id_toString:
            return realThis(thisObj, f).toString();
          case Id_toXMLString:
            return realThis(thisObj, f).toXMLString();
          case Id_valueOf:
            return realThis(thisObj, f).valueOf();
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    /**
     *
     * @param thisObj
     * @param f
     * @return
     */
    private XMLObjectImpl realThis(Scriptable thisObj, IdFunctionObject f)
    {
        if(!(thisObj instanceof XMLObjectImpl))
            throw incompatibleCallError(f);
        return (XMLObjectImpl)thisObj;
    }

    private static Object arg(Object[] args, int i)
    {
        return (i < args.length) ? args[i] : Undefined.instance;
    }

    private XMLName getXmlNameOrIndex(Context cx, Object[] args, int i)
    {
        return lib.toXMLNameOrIndex(cx, arg(args, i));
    }

}