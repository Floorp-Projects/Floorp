/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <mhammond@skippinet.com.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef nsIScriptTimeoutHandler_h___
#define nsIScriptTimeoutHandler_h___

class nsIArray;

#define NS_ISCRIPTTIMEOUTHANDLER_IID \
{ 0xcaf520a5, 0x8078, 0x4cba, \
  { 0x8a, 0xb9, 0xb6, 0x8a, 0x12, 0x43, 0x4f, 0x05 } }

/**
 * Abstraction of the script objects etc required to do timeouts in a
 * language agnostic way.
 */

class nsIScriptTimeoutHandler : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTTIMEOUTHANDLER_IID)

  // Get a script object for the language suitable for passing back to
  // the language's context as an event handler.  If this returns nsnull,
  // GetHandlerText() will be called to get the string.
  virtual JSObject *GetScriptObject() = 0;

  // Get the handler text of not a compiled object.
  virtual const PRUnichar *GetHandlerText() = 0;

  // Get the location of the script.
  // Note: The memory pointed to by aFileName is owned by the
  // nsIScriptTimeoutHandler and should not be freed by the caller.
  virtual void GetLocation(const char **aFileName, PRUint32 *aLineNo) = 0;

  // If a script object, get the argv suitable for passing back to the
  // script context.
  virtual nsIArray *GetArgv() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptTimeoutHandler,
                              NS_ISCRIPTTIMEOUTHANDLER_IID)

#endif // nsIScriptTimeoutHandler_h___
