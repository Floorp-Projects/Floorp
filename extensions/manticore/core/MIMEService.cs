
namespace Silverstone.Manticore.Core
{
  using System;

  using Microsoft.Win32;

	/// <summary>
	/// Summary description for MIMEService.
	/// </summary>
	public class MIMEService
	{
    /// <summary>
    /// Retrieves the extension associated with the specified content type
    /// by looking up the type in the Windows Registry. 
    /// </summary>
    /// <param name="aMIMEType">content-type to retrieve extension for</param>
    /// <returns>string containing the extension (".foo") associated with the type</returns>
    public static string GetExtensionForMIMEType(string aMIMEType)
    {
      RegistryKey clsRoot = Registry.ClassesRoot;
      string extFromMIMEDBKey = "MIME\\Database\\Content Type\\" + aMIMEType;
      RegistryKey extensionKey = clsRoot.OpenSubKey(extFromMIMEDBKey);
      return extensionKey.GetValue("Extension") as String;
    }

    /// <summary>
    /// Retrieves the pretty user-readable description of the type by
    /// looking up the type in the Windows Registry. If no data is found,
    /// the extension is capitalized and appended with "file", e.g.
    /// "ZAP file"
    /// </summary>
    /// <param name="aMIMEType">content-type to retrieve description for</param>
    /// <returns>string containing pretty user-readable description of type</returns>
    public static string GetDescriptionForMIMEType(string aMIMEType)
    {
      RegistryKey clsRoot = Registry.ClassesRoot;
      string extension = GetExtensionForMIMEType(aMIMEType);
      RegistryKey handlerKey = clsRoot.OpenSubKey(extension);
      string handler = handlerKey.GetValue("") as String;
      RegistryKey descriptionKey = clsRoot.OpenSubKey(handler);
      string description = descriptionKey.GetValue("") as String;
      if (description == "")
        description = extension.Substring(1,extension.Length-1).ToUpper() + " file";
      return description;
    }
	}
}
