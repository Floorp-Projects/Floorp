/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/******

  This file contains the list of all XBL nsIAtoms and their values
  
  It is designed to be used as inline input to nsXBLAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro XBL_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to XBL_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


XBL_ATOM(binding, "binding")
XBL_ATOM(bindings, "bindings")
XBL_ATOM(handlers, "handlers")
XBL_ATOM(handler, "handler")
XBL_ATOM(resources, "resources")
XBL_ATOM(image, "image")
XBL_ATOM(stylesheet, "stylesheet")
XBL_ATOM(implementation, "implementation")
XBL_ATOM(implements, "implements")
XBL_ATOM(xbltext, "xbl:text")
XBL_ATOM(method, "method")
XBL_ATOM(property, "property")
XBL_ATOM(field, "field")
XBL_ATOM(event, "event")
XBL_ATOM(phase, "phase")
XBL_ATOM(action, "action")
XBL_ATOM(command, "command")
XBL_ATOM(modifiers, "modifiers")
XBL_ATOM(clickcount, "clickcount")
XBL_ATOM(charcode, "charcode")
XBL_ATOM(keycode, "keycode")
XBL_ATOM(key, "key")
XBL_ATOM(onget, "onget")
XBL_ATOM(onset, "onset")
XBL_ATOM(name, "name")
XBL_ATOM(getter, "getter")
XBL_ATOM(setter, "setter")
XBL_ATOM(body, "body")
XBL_ATOM(readonly, "readonly")
XBL_ATOM(parameter, "parameter")
XBL_ATOM(children, "children")
XBL_ATOM(extends, "extends")
XBL_ATOM(display, "display")
XBL_ATOM(inherits, "inherits")
XBL_ATOM(includes, "includes")
XBL_ATOM(excludes, "excludes")
XBL_ATOM(content, "content")
XBL_ATOM(constructor, "constructor")
XBL_ATOM(destructor, "destructor")
XBL_ATOM(inheritstyle, "inheritstyle")



