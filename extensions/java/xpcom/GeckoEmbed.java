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

import java.io.*;

/**
 *  Provides access to functions that are used to embed Mozilla's Gecko layer.
 */
public final class GeckoEmbed {

  /**
   * Initializes the Gecko embedding layer. You <i>must</i>
   * call this method before proceeding to use Gecko. This function ensures
   * XPCOM is started, creates the component registry if necessary and
   * starts global services.
   *
   * @param aMozBinDirectory The Gecko directory containing the component
   *                         registry and runtime libraries;
   *                         or use <code>null</code> to use the working
   *                         directory.
   * @param aAppFileLocProvider The object to be used by Gecko that specifies
   *                         to Gecko where to find profiles, the component
   *                         registry preferences and so on; or use
   *                         <code>null</code> for the default behaviour.
   *
   * @exception XPCOMException  if a failure occurred during initialization
   */
  public static native
  void initEmbedding(File aMozBinDirectory,
                     AppFileLocProvider aAppFileLocProvider);

  /**
   * Terminates the Gecko embedding layer. Call this function during shutdown to
   * ensure that global services are unloaded, files are closed and
   * XPCOM is shutdown.
   * <p>
   * NOTE: Release any XPCOM objects within Gecko that you may be holding a
   *       reference to before calling this function.
   * </p>
   *
   * @exception XPCOMException  if a failure occurred during termination
   */
  public static native
  void termEmbedding();

}
