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
 *   Frank Mitchell
 *   Mike Shaver
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

package org.mozilla.javascript;

/**
 * This class reflects a single Java constructor into the JavaScript
 * environment.  It satisfies a request for an overloaded constructor,
 * as introduced in LiveConnect 3.
 * All NativeJavaConstructors behave as JSRef `bound' methods, in that they
 * always construct the same NativeJavaClass regardless of any reparenting
 * that may occur.
 *
 * @author Frank Mitchell
 * @see NativeJavaMethod
 * @see NativeJavaPackage
 * @see NativeJavaClass
 */

public class NativeJavaConstructor extends BaseFunction
{
    static final long serialVersionUID = -8149253217482668463L;

    MemberBox ctor;

    public NativeJavaConstructor(MemberBox ctor)
    {
        this.ctor = ctor;
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        return NativeJavaClass.constructSpecific(cx, scope, args, ctor);
    }

    public String getFunctionName()
    {
        String sig = JavaMembers.liveConnectSignature(ctor.argTypes);
        return "<init>".concat(sig);
    }

    public String toString()
    {
        return "[JavaConstructor " + ctor.getName() + "]";
    }
}

