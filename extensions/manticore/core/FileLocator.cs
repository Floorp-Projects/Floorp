/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Manticore.
 *
 * The Initial Developer of the Original Code is
 * Silverstone Interactive.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
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

namespace Silverstone.Manticore.Core
{
  using System;
  using System.Runtime.InteropServices;
  using System.Text;
  using System.Windows.Forms;

  /// <summary>
  /// Locate special folders
  /// </summary>
  public class FileLocator
  {
    [DllImport("shfolder.dll")]
    static extern int SHGetFolderPath(int aOwningWnd, int nFolder, int aToken, int aFlags, StringBuilder buffer);

    /// <summary>
    /// All the special folders. This is just copied from Windows headers so that clients
    /// can use this namespace and obtain folder ids without having to use Shell32.dll
    /// </summary>
    public enum SpecialFolders : uint
    {
      ssfDESKTOP = 0,
      ssfPROGRAMS = 0x2,
      ssfCONTROLS = 0x3,
      ssfPRINTERS = 0x4,
      ssfPERSONAL = 0x5,
      ssfFAVORITES = 0x6,
      ssfSTARTUP = 0x7,
      ssfRECENT = 0x8,
      ssfSENDTO = 0x9,
      ssfBITBUCKET = 0xa,
      ssfSTARTMENU = 0xb,
      ssfDESKTOPDIRECTORY = 0x10,
      ssfDRIVES = 0x11,
      ssfNETWORK = 0x12,
      ssfNETHOOD = 0x13,
      ssfFONTS = 0x14,
      ssfTEMPLATES = 0x15,
      ssfCOMMONSTARTMENU = 0x16,
      ssfCOMMONPROGRAMS = 0x17,
      ssfCOMMONSTARTUP = 0x18,
      ssfCOMMONDESKTOPDIR = 0x19,
      ssfAPPDATA = 0x1a,
      ssfPRINTHOOD = 0x1b,
      ssfLOCALAPPDATA = 0x1c,
      ssfALTSTARTUP = 0x1d,
      ssfCOMMONALTSTARTUP = 0x1e,
      ssfCOMMONFAVORITES = 0x1f,
      ssfINTERNETCACHE = 0x20,
      ssfCOOKIES = 0x21,
      ssfHISTORY = 0x22,
      ssfCOMMONAPPDATA = 0x23,
      ssfWINDOWS = 0x24,
      ssfSYSTEM = 0x25,
      ssfPROGRAMFILES = 0x26,
      ssfMYPICTURES = 0x27,
      ssfPROFILE = 0x28
    }

    public static String GetFolderPath(SpecialFolders aFolder)
    {
      StringBuilder builder = new StringBuilder(256);
      SHGetFolderPath(0, (int) aFolder, 0, 0, builder);
      return builder.ToString();
    }

    public static String GetManticorePath(String aPath)
    {
      String path = "";
      String appData = "";
      switch (aPath) 
      {
        case "AppData":
          path = FileLocator.GetFolderPath(FileLocator.SpecialFolders.ssfAPPDATA);
          path += @"\Manticore\";
          break;
        case "UserPrefs":
          appData = FileLocator.GetManticorePath("AppData");
          path += appData + @"user-prefs.xml";
          break;
        case "LocalBookmarks":
          appData = FileLocator.GetManticorePath("AppData");
          if (appData != "")
            path += appData + @"bookmarks.xml";
          break;
        case "Application":
          path = Application.ExecutablePath;
          int lastSlash = path.LastIndexOf(@"\");
          path = path.Substring(0, lastSlash + 1);
          break;
      }
      return path;
    }
  }
}
