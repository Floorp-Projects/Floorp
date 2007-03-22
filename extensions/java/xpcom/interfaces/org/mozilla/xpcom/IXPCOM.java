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

import org.mozilla.interfaces.nsIComponentManager;
import org.mozilla.interfaces.nsIComponentRegistrar;
import org.mozilla.interfaces.nsILocalFile;
import org.mozilla.interfaces.nsIServiceManager;


public interface IXPCOM {

  /**
   * Initializes XPCOM. You must call this method before proceeding
   * to use XPCOM.
   *
   * @param aMozBinDirectory The directory containing the component
   *                         registry and runtime libraries;
   *                         or use <code>null</code> to use the working
   *                         directory.
   *
   * @param aAppFileLocProvider The object to be used by Gecko that specifies
   *                         to Gecko where to find profiles, the component
   *                         registry preferences and so on; or use
   *                         <code>null</code> for the default behaviour.
   *
   * @return the service manager
   *
   * @throws XPCOMException <ul>
   *      <li> NS_ERROR_NOT_INITIALIZED - if static globals were not initialied,
   *            which can happen if XPCOM is reloaded, but did not completly
   *            shutdown. </li>
   *      <li> Other error codes indicate a failure during initialisation. </li>
   * </ul>
   */
  nsIServiceManager initXPCOM(File aMozBinDirectory,
          IAppFileLocProvider aAppFileLocProvider) throws XPCOMException;

  /**
   * Shutdown XPCOM. You must call this method after you are finished
   * using xpcom.
   *
   * @param aServMgr    The service manager which was returned by initXPCOM.
   *                    This will release servMgr.
   *
   * @throws XPCOMException  if a failure occurred during termination
   */
  void shutdownXPCOM(nsIServiceManager aServMgr) throws XPCOMException;

  /**
   * Public Method to access to the service manager.
   *
   * @return the service manager
   *
   * @throws XPCOMException
   */
  nsIServiceManager getServiceManager() throws XPCOMException;

  /**
   * Public Method to access to the component manager.
   *
   * @return the component manager
   *
   * @throws XPCOMException
   */
  nsIComponentManager getComponentManager() throws XPCOMException;

  /**
   * Public Method to access to the component registration manager.
   * 
   * @return the component registration manager
   *
   * @throws XPCOMException
   */
  nsIComponentRegistrar getComponentRegistrar() throws XPCOMException;

  /**
   * Public Method to create an instance of a nsILocalFile.
   *
   * @param aPath         A string which specifies a full file path to a 
   *                      location.  Relative paths will be treated as an
   *                      error (NS_ERROR_FILE_UNRECOGNIZED_PATH).
   * @param aFollowLinks  This attribute will determine if the nsLocalFile will
   *                      auto resolve symbolic links.  By default, this value
   *                      will be false on all non unix systems.  On unix, this
   *                      attribute is effectively a noop.
   *
   * @return an instance of an nsILocalFile that points to given path
   *
   * @throws XPCOMException <ul>
   *      <li> NS_ERROR_FILE_UNRECOGNIZED_PATH - raised for unrecognized paths
   *           or relative paths (must supply full file path) </li>
   * </ul>
   */
  nsILocalFile newLocalFile(String aPath, boolean aFollowLinks)
          throws XPCOMException;

}

