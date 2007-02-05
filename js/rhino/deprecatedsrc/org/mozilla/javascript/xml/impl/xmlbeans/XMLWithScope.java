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
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
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

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

final class XMLWithScope extends NativeWith
{
    private static final long serialVersionUID = -696429282095170887L;

    private XMLLibImpl lib;
    private int         _currIndex;
    private XMLList     _xmlList;
    private XMLObject   _dqPrototype;

    XMLWithScope(XMLLibImpl lib, Scriptable parent, XMLObject prototype)
    {
        super(parent, prototype);
        this.lib = lib;
    }

    void initAsDotQuery()
    {
        XMLObject prototype = (XMLObject)getPrototype();
        // XMLWithScope also handles the .(xxx) DotQuery for XML
        // basically DotQuery is a for/in/with statement and in
        // the following 3 statements we setup to signal it's
        // DotQuery,
        // the index and the object being looped over.  The
        // xws.setPrototype is the scope of the object which is
        // is a element of the lhs (XMLList).
        _currIndex = 0;
        _dqPrototype = prototype;
        if (prototype instanceof XMLList) {
            XMLList xl = (XMLList)prototype;
            if (xl.length() > 0) {
                setPrototype((Scriptable)(xl.get(0, null)));
            }
        }
        // Always return the outer-most type of XML lValue of
        // XML to left of dotQuery.
        _xmlList = new XMLList(lib);
    }

    protected Object updateDotQuery(boolean value)
    {
        // Return null to continue looping

        XMLObject seed = _dqPrototype;
        XMLList xmlL = _xmlList;

        if (seed instanceof XMLList) {
            // We're a list so keep testing each element of the list if the
            // result on the top of stack is true then that element is added
            // to our result list.  If false, we try the next element.
            XMLList orgXmlL = (XMLList)seed;

            int idx = _currIndex;

            if (value) {
                xmlL.addToList(orgXmlL.get(idx, null));
            }

            // More elements to test?
            if (++idx < orgXmlL.length()) {
                // Yes, set our new index, get the next element and
                // reset the expression to run with this object as
                // the WITH selector.
                _currIndex = idx;
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
