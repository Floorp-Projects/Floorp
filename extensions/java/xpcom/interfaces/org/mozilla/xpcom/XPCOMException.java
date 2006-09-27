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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
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

package org.mozilla.xpcom;


/**
 * This exception is thrown whenever an internal XPCOM/Gecko error occurs.
 * You can query the error ID returned by XPCOM by checking
 * <code>errorcode</code> field.
 */
public class XPCOMException extends RuntimeException {

  /**
   * The XPCOM error value.
   */
  public long errorcode;

  private static final long serialVersionUID = 198521829884000593L;

  /**
   * Constructs a new XPCOMException instance, with a default error
   * (NS_ERROR_FAILURE) and message.
   */
  public XPCOMException() {
    this(0x80004005L, "Unspecified internal XPCOM error");
  }

  /**
   * Constructs a new XPCOMException instance with the given message, passing
   * NS_ERROR_FAILURE as the error code.
   *
   * @param message   detailed message of exception
   */
  public XPCOMException(String message) {
    this(0x80004005L, message);
  }

  /**
   * Constructs a new XPCOMException instance with the given code, passing
   * a default message.
   *
   * @param code      internal XPCOM error ID
   */
  public XPCOMException(long code) {
    this(code, "Internal XPCOM error");
  }

  /**
   * Constructs a new XPCOMException instance with an error code and message.
   *
   * @param code      internal XPCOM error ID
   * @param message   detailed message of exception
   */
  public XPCOMException(long code, String message) {
    super(message + "  (0x" + Long.toHexString(code) + ")");
    this.errorcode = code;
  }

}

