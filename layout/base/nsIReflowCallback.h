/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David Hyatt (hyatt@netscape.com)
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
#ifndef nsIReflowCallback_h___
#define nsIReflowCallback_h___

/**
 * Reflow callback interface.
 * These are not refcounted. Objects must be removed from the presshell
 * callback list before they die.
 * Protocol: objects will either get a ReflowFinished() call when a reflow
 * has finished or a ReflowCallbackCanceled() call if the shell is destroyed,
 * whichever happens first. If the object is explicitly removed from the shell
 * (using nsIPresShell::CancelReflowCallback()) before that occurs then neither
 * of the callback methods are called.
 */
class nsIReflowCallback {
public:
  /**
   * The presshell calls this when reflow has finished. Return PR_TRUE if
   * you need a Flush_Layout to happen after this.
   */
  virtual bool ReflowFinished() = 0;
  /**
   * The presshell calls this on outstanding callback requests in its
   * Destroy() method. The shell removes the request after calling
   * ReflowCallbackCanceled().
   */
  virtual void ReflowCallbackCanceled() = 0;
};

#endif /* nsIReflowCallback_h___ */
