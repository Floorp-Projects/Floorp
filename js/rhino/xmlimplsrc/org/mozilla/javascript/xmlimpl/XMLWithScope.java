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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
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

final class XMLWithScope extends NativeWith
{
    private XMLLib lib;
    private int         _currIndex;
    private XMLList _xmlList = null;
    private Scriptable  _dqPrototype;

    XMLWithScope(XMLLib lib, Scriptable parent, XMLObject prototype)
    {
        super(parent, prototype);
        this.lib = lib;
    }

    public XMLLib lib()
    {
        return lib;
    }

    public void setCurrIndex (int idx)
    {
        _currIndex = idx;
    }

    public int getCurrIndex ()
    {
        return _currIndex;
    }

    public void setXMLList (XMLList l)
    {
        _xmlList = l;
    }

    public Scriptable getDQPrototype() {
        return _dqPrototype;
    }

    public void setDQPrototype(Scriptable dqPrototype) {
        _dqPrototype = dqPrototype;
    }

    public Object updateDotQuery(boolean value)
    {
        // Return null to continue looping

        Scriptable seed = getDQPrototype();
        XMLList xmlL = _xmlList;

        Object result;
        if (seed instanceof XMLList) {
            // We're a list so keep testing each element of the list if the
            // result on the top of stack is true then that element is added
            // to our result list.  If false, we try the next element.
            XMLList orgXmlL = (XMLList)seed;

            int idx = getCurrIndex();

            if (value) {
                xmlL.addToList(orgXmlL.get(idx, null));
            }

            // More elements to test?
            if (++idx < orgXmlL.length()) {
                // Yes, set our new index, get the next element and
                // reset the expression to run with this object as
                // the WITH selector.
                setCurrIndex(idx);
                setPrototype((Scriptable)(orgXmlL.get(idx, null)));

                // continue looping
                return null;
            }
        } else {
            // If we're not a XMLList then there's no looping
            // just return DQPrototype if the result is true.
            if (value) {
              xmlL.addToList(seed);
            }
        }

        return xmlL;
    }
}
