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

package org.mozilla.xpcom.internal;

import java.io.*;
import org.mozilla.xpcom.*;


public class GREImpl implements IGRE {

  public void initEmbedding(File aLibXULDirectory, File aAppDirectory,
                            IAppFileLocProvider aAppDirProvider) {
    loadDependentLibraries(aLibXULDirectory);
    initEmbeddingNative(aLibXULDirectory, aAppDirectory, aAppDirProvider);
  }

  private void loadDependentLibraries(File binPath) {
    String path = "";
    if (binPath != null) {
      path = binPath + File.separator;
    }

    System.load(path + System.mapLibraryName("nspr4"));
    System.load(path + System.mapLibraryName("plds4"));
    System.load(path + System.mapLibraryName("plc4"));
    try {
      /* try loading Win32 DLL */
      System.load(path + System.mapLibraryName("js3250"));
    } catch (UnsatisfiedLinkError e) { }
    try {
      /* try loading Linux DLL */
      System.load(path + System.mapLibraryName("mozjs"));
    } catch (UnsatisfiedLinkError e) { }
    System.load(path + System.mapLibraryName("softokn3"));
    System.load(path + System.mapLibraryName("nss3"));
    System.load(path + System.mapLibraryName("smime3"));
    System.load(path + System.mapLibraryName("ssl3"));
    System.load(path + System.mapLibraryName("xul"));
    System.load(path + System.mapLibraryName("xpcom"));

    System.load(path + System.mapLibraryName("javaxpcom"));
  }

  public native void initEmbeddingNative(File aLibXULDirectory,
          File aAppDirectory, IAppFileLocProvider aAppDirProvider);

  public native void termEmbedding();

}

