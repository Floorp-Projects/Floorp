/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* jband Sept 1998 - **NOT original file** - copied here for simplicity */

/**
 * The JSProxy interface allows applets and plugins to
 * share javascript contexts.
 */

package netscape.javascript;
import java.applet.Applet;

public interface JSProxy {
    Object  getMember(JSObject jso, String name);
    Object  getSlot(JSObject jso, int index);
    void    setMember(JSObject jso, String name, Object value);
    void    setSlot(JSObject jso, int index, Object value);
    void    removeMember(JSObject jso, String name);
    Object  call(JSObject jso, String methodName, Object args[]);
    Object  eval(JSObject jso, String s);
    String      toString(JSObject jso);
    JSObject    getWindow(Applet applet);
}
