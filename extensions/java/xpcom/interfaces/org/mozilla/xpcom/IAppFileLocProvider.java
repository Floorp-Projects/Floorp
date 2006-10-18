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

import java.io.File;


/**
 * Used by XPCOM's Directory Service to get file locations.
 * <p>
 * This interface is similar to <code>nsIDirectoryServiceProvider</code> and
 * <code>nsIDirectoryServiceProvider2</code>, except that its methods use
 * <code>java.io.File</code> instead of <code>nsIFile</code>.
 * </p>
 *
 * @see Mozilla#initEmbedding
 * @see Mozilla#initXPCOM
 * @see <a href=
 *     "http://lxr.mozilla.org/mozilla/source/xpcom/io/nsIDirectoryService.idl">
 *      nsIDirectoryServiceProvider </a>
 * @see <a href=
 *    "http://lxr.mozilla.org/mozilla/source/xpcom/io/nsDirectoryServiceDefs.h">
 *      Directory Service property names </a>
 */
public interface IAppFileLocProvider {

  /**
   * Directory Service calls this when it gets the first request for
   * a property or on every request if the property is not persistent.
   *
   * @param prop        the symbolic name of the file
   * @param persistent  an array of length one used to supply the output value:
   *                    <ul>
   *                      <li><code>true</code> - The returned file will be
   *                      cached by Directory Service. Subsequent requests for
   *                      this prop will bypass the provider and use the cache.
   *                      </li>
   *                      <li><code>false</code> - The provider will be asked
   *                      for this prop each time it is requested. </li>
   *                    </ul>
   *
   * @return            the file represented by the property
   */
  File getFile(String prop, boolean[] persistent);

  /**
   * Directory Service calls this when it gets a request for
   * a property and the requested type is nsISimpleEnumerator.
   *
   * @param prop  the symbolic name of the file list
   *
   * @return      an array of file locations
   */
  File[] getFiles(String prop);

}

