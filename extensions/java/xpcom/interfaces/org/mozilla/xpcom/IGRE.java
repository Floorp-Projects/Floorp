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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

import java.io.File;


public interface IGRE {

  /**
   * Initializes libXUL for embedding purposes.
   * <p>
   * NOTE: This function must be called from the "main" thread.
   * <p>
   * NOTE: At the present time, this function may only be called once in
   *       a given process. Use <code>termEmbedding</code> to clean up and free
   *       resources allocated by <code>initEmbedding</code>.
   *
   * @param aLibXULDirectory   The directory in which the libXUL shared library
   *                           was found.
   * @param aAppDirectory      The directory in which the application components
   *                           and resources can be found. This will map to
   *                           the "resource:app" directory service key.
   * @param aAppDirProvider    A directory provider for the application. This
   *                           provider will be aggregated by a libXUL provider
   *                           which will provide the base required GRE keys.
   *
   * @throws XPCOMException  if a failure occurred during initialization
   */
  void initEmbedding(File aLibXULDirectory, File aAppDirectory,
          IAppFileLocProvider aAppDirProvider) throws XPCOMException;

  /**
   * Terminates libXUL embedding.
   * <p>
   * NOTE: Release any references to XPCOM objects that you may be holding
   *       before calling this function.
   */
  void termEmbedding();

  /**
   * Lock a profile directory using platform-specific semantics.
   *
   * @param aDirectory  The profile directory to lock.
   *
   * @return  A lock object. The directory will remain locked until the lock is
   *          released by invoking the <code>release</code> method, or by the
   *          termination of the JVM, whichever comes first.
   *
   * @throws XPCOMException if a failure occurred
   */
  ProfileLock lockProfileDirectory(File aDirectory) throws XPCOMException;

 /**
   * Fire notifications to inform the toolkit about a new profile. This
   * method should be called after <code>initEmbedding</code> if the
   * embedder wishes to run with a profile.
   * <p>
   * Normally the embedder should call <code>lockProfileDirectory</code>
   * to lock the directory before calling this method.
   * <p>
   * NOTE: There are two possibilities for selecting a profile:
   * <ul>
   * <li>
   *    Select the profile before calling <code>initEmbedding</code>.
   *    The aAppDirProvider object passed to <code>initEmbedding</code>
   *    should provide the NS_APP_USER_PROFILE_50_DIR key, and
   *    may also provide the following keys:
   *      <ul>
   *        <li>NS_APP_USER_PROFILE_LOCAL_50_DIR
   *        <li>NS_APP_PROFILE_DIR_STARTUP
   *        <li>NS_APP_PROFILE_LOCAL_DIR_STARTUP
   *      </ul>
   *    In this scenario <code>notifyProfile</code> should be called
   *    immediately after <code>initEmbedding</code>. Component
   *    registration information will be stored in the profile and
   *    JS components may be stored in the fastload cache.
   * </li>
   * <li>
   * 	Select a profile some time after calling <code>initEmbedding</code>.
   *    In this case the embedder must install a directory service 
   *    provider which provides NS_APP_USER_PROFILE_50_DIR and optionally
   *    NS_APP_USER_PROFILE_LOCAL_50_DIR. Component registration information
   *    will be stored in the application directory and JS components will not
   *    fastload.
   * </li>
   * </ul>
   */
  void notifyProfile();

}


