/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* ** */
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
