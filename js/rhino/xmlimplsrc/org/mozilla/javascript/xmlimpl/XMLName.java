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

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Kit;
import org.mozilla.javascript.Ref;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Undefined;

class XMLName extends Ref
{
    private String uri;
    private String localName;
    private boolean isAttributeName;
    private boolean isDescendants;

    private XMLName(String uri, String localName)
    {
        this.uri = uri;
        this.localName = localName;
    }

    static XMLName formStar()
    {
        return new XMLName(null, "*");
    }

    static XMLName formProperty(String uri, String localName)
    {
        return new XMLName(uri, localName);
    }

    String uri()
    {
        return uri;
    }

    String localName()
    {
        return localName;
    }

    boolean isAttributeName()
    {
        return isAttributeName;
    }

    void setAttributeName()
    {
        if (isAttributeName) throw new IllegalStateException();
        isAttributeName = true;
    }

    boolean isDescendants()
    {
        return isDescendants;
    }

    void setIsDescendants()
    {
        if (isDescendants) throw new IllegalStateException();
        isDescendants = true;
    }

    public boolean has(Context cx, Scriptable target)
    {
        XMLObjectImpl xmlObject = (XMLObjectImpl)target;
        if (xmlObject == null) {
            return false;
        }
        return xmlObject.hasXMLProperty(this);
    }

    public Object get(Context cx, Scriptable target)
    {
        XMLObjectImpl xmlObject = (XMLObjectImpl)target;
        if (xmlObject == null) {
            throw ScriptRuntime.undefReadError(Undefined.instance,
                                               toString());
        }
        return xmlObject.getXMLProperty(this);
    }

    public Object set(Context cx, Scriptable target, Object value)
    {
        XMLObjectImpl xmlObject = (XMLObjectImpl)target;
        if (xmlObject == null) {
            throw ScriptRuntime.undefWriteError(Undefined.instance,
                                                toString(),
                                                value);
        }
        // Assignment to descendants causes parse error on bad reference
        // and this should not be called
        if (isDescendants) throw Kit.codeBug();
        xmlObject.putXMLProperty(this, value);
        return value;
    }

    public boolean delete(Context cx, Scriptable target)
    {
        XMLObjectImpl xmlObject = (XMLObjectImpl)target;
        if (xmlObject == null) {
            return true;
        }
        xmlObject.deleteXMLProperty(this);
        return !xmlObject.hasXMLProperty(this);
    }

    public String toString()
    {
        //return qname.localName();
        StringBuffer buff = new StringBuffer();
        if (isDescendants) buff.append("..");
        if (isAttributeName) buff.append('@');
        if (uri == null) {
            buff.append('*');
            if(localName().equals("*")) {
                return buff.toString();
            }
        } else {
            buff.append('"').append(uri()).append('"');
        }
        buff.append(':').append(localName());
        return buff.toString();
    }

}