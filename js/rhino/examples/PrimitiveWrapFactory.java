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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
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

import org.mozilla.javascript.*;

/**
 * An example WrapFactory that can be used to avoid wrapping of Java types
 * that can be converted to ECMA primitive values.
 * So java.lang.String is mapped to ECMA string, all java.lang.Numbers are
 * mapped to ECMA numbers, and java.lang.Booleans are mapped to ECMA booleans
 * instead of being wrapped as objects. Additionally java.lang.Character is
 * converted to ECMA string with length 1.
 * Other types have the default behavior.
 * <p>
 * Note that calling "new java.lang.String('foo')" in JavaScript with this
 * wrap factory enabled will still produce a wrapped Java object since the
 * WrapFactory.wrapNewObject method is not overridden.
 * <p>
 * The PrimitiveWrapFactory is enabled on a Context by calling setWrapFactory
 * on that context.
 */
public class PrimitiveWrapFactory extends WrapFactory {

  public Object wrap(Context cx, Scriptable scope, Object obj,
                     Class staticType)
  {
    if (obj instanceof String || obj instanceof Number ||
        obj instanceof Boolean)
    {
      return obj;
    } else if (obj instanceof Character) {
      char[] a = { ((Character)obj).charValue() };
      return new String(a);
    }
    return super.wrap(cx, scope, obj, staticType);
  }
}
