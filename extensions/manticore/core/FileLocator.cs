
namespace Silverstone.Manticore.Core
{
  using System;
  using System.Runtime.InteropServices;
  using System.Text;

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
      ssfPROFILE = 0x28,
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
      switch (aPath) {
        case "AppData":
          path = FileLocator.GetFolderPath(FileLocator.SpecialFolders.ssfAPPDATA);
          path += @"\Manticore\";
          break;
        case "UserPrefs":
          String appData = FileLocator.GetManticorePath("AppData");
          path += appData + @"user-prefs.xml";
          break;
      }
      return path;
    }
  }
}
