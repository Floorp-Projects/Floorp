
namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.Windows.Forms;

  /// <summary>
	/// Top level window class
	/// </summary>
	public class ManticoreWindow : Form
	{
    protected internal String mType = "";

    public String Type
    {
      get {
        return mType;
      }
    }

    public ManticoreWindow()
    {
    }

    public ManticoreWindow(String aType)
		{
      mType = aType;
		}
	}
}
