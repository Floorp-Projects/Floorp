
namespace Silverstone.Manticore.App 
{
  using System;
  using System.ComponentModel;
  using System.Windows.Forms;
  using System.Collections;

  using Silverstone.Manticore.BrowserWindow;

  public class ManticoreApp
  {
    // XXX Need to do something here more similar
    //     to what mozilla does here for parameterized
    //     window types.
    private Queue browserWindows;

    public ManticoreApp()
    {
      browserWindows = new Queue();

      OpenNewBrowser();

      Application.Run();
    }

    // Opens and displays a new browser window
    public void OpenNewBrowser()
    {
      BrowserWindow window = new BrowserWindow(this);
      browserWindows.Enqueue(window);
      window.Show();
    }

    public static void Main(string[] args) 
    {
      ManticoreApp app = new ManticoreApp();
    }
  }
}

