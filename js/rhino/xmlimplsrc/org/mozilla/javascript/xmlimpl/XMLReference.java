/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is XMLReference class for E4X extension of Rhino.
 *
 * The Initial Developer of the Original Code is
 * RUnit Software AS.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov, igor@fastmail.fm
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

package org.mozilla.javascript.xmlimpl;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

class XMLReference extends Reference
{
    private boolean descendants;
    private XMLObjectImpl xmlObject;
    private XMLName xmlName;

    /**
     * When xmlObject == null, it corresponds to undefined in JS, not null
     */
    XMLReference(boolean descendants, XMLObjectImpl xmlObject, XMLName xmlName)
    {
        if (xmlName == null)
            throw new IllegalArgumentException();
        if (descendants && xmlObject == null)
            throw new IllegalArgumentException();

        this.descendants = descendants;
        this.xmlObject = xmlObject;
        this.xmlName = xmlName;
    }

    public boolean has(Context cx)
    {
        if (xmlObject == null) {
            return false;
        }
        return xmlObject.hasXMLProperty(xmlName, descendants);
    }

    /**
     * See E4X 11.1 PrimaryExpression : PropertyIdentifier production
     */
    public Object get(Context cx)
    {
        if (xmlObject == null) {
            throw ScriptRuntime.undefReadError(Undefined.instance,
                                               xmlName.toString());
        }
        return xmlObject.getXMLProperty(xmlName, descendants);
    }

    public Object set(Context cx, Object value)
    {
        if (xmlObject == null) {
            throw ScriptRuntime.undefWriteError(Undefined.instance,
                                                xmlName.toString(),
                                                value);
        }
        // Assignment to descendants causes parse error on bad reference
        // and this should not be called
        if (descendants) throw Kit.codeBug();
        xmlObject.putXMLProperty(xmlName, value);
        return value;
    }

    public boolean delete(Context cx)
    {
        if (xmlObject != null) {
            xmlObject.deleteXMLProperty(xmlName, descendants);
        }
        return !has(cx);
    }
}

