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

public final class GeckoEmbed {

  // XPCOM Utility functions
  public static native void NS_InitEmbedding(String aMozBinDirectory, int aAppFileLocProvider);
  public static native void NS_TermEmbedding();
  public static native nsILocalFile NS_NewLocalFile(String aPath, boolean followLinks);
  public static native nsIComponentManager NS_GetComponentManager();
  public static native nsIServiceManager NS_GetServiceManager();
  public static native nsISimpleEnumerator NS_NewSingletonEnumerator(nsISupports aSingleton);

  // Calling functions used by Java stub classes
  public static native void CallXPCOMMethodVoid(Object thisObj, int fnNumber, Object[] params);
  public static native boolean CallXPCOMMethodBool(Object thisObj, int fnNumber, Object[] params);
  public static native boolean[] CallXPCOMMethodBoolA(Object thisObj, int fnNumber, Object[] params);
  public static native byte CallXPCOMMethodByte(Object thisObj, int fnNumber, Object[] params);
  public static native byte[] CallXPCOMMethodByteA(Object thisObj, int fnNumber, Object[] params);
  public static native char CallXPCOMMethodChar(Object thisObj, int fnNumber, Object[] params);
  public static native char[] CallXPCOMMethodCharA(Object thisObj, int fnNumber, Object[] params);
  public static native short CallXPCOMMethodShort(Object thisObj, int fnNumber, Object[] params);
  public static native short[] CallXPCOMMethodShortA(Object thisObj, int fnNumber, Object[] params);
  public static native int CallXPCOMMethodInt(Object thisObj, int fnNumber, Object[] params);
  public static native int[] CallXPCOMMethodIntA(Object thisObj, int fnNumber, Object[] params);
  public static native long CallXPCOMMethodLong(Object thisObj, int fnNumber, Object[] params);
  public static native long[] CallXPCOMMethodLongA(Object thisObj, int fnNumber, Object[] params);
  public static native float CallXPCOMMethodFloat(Object thisObj, int fnNumber, Object[] params);
  public static native float[] CallXPCOMMethodFloatA(Object thisObj, int fnNumber, Object[] params);
  public static native double CallXPCOMMethodDouble(Object thisObj, int fnNumber, Object[] params);
  public static native double[] CallXPCOMMethodDoubleA(Object thisObj, int fnNumber, Object[] params);
  public static native Object CallXPCOMMethodObj(Object thisObj, int fnNumber, Object[] params);
  public static native Object[] CallXPCOMMethodObjA(Object thisObj, int fnNumber, Object[] params);

  public static native void FinalizeStub(Object thisObj);

  public static native int nsWriteSegmentFun(int ptr, Object aInStream, int aClosure, byte[] aFromSegment, int aToOffset, int aCount);
}