
namespace Silverstone.Manticore.App 
{
  using System;
  using System.ComponentModel;
  using System.Windows.Forms;
  using System.Collections;

  using Silverstone.Manticore.Browser;
  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.Bookmarks;

  public class ManticoreApp
  {
    // XXX Need to do something here more similar
    //     to what mozilla does here for parameterized
    //     window types.
    private Hashtable mBrowserWindows;
    private Preferences mPreferences;
    private Bookmarks mBookmarks;

    public Preferences Prefs {
      get {
        return mPreferences;
      }
    }

    public Bookmarks BM {
      get {
        return mBookmarks;
      }
    }

    public ManticoreApp()
    {
      mBrowserWindows = new Hashtable();

      // Initialize default and user preferences
      mPreferences = new Preferences();
      mPreferences.InitializeDefaults();
      mPreferences.LoadUserPreferences();

      // Initialize bookmarks
      mBookmarks = new Bookmarks(this);
      mBookmarks.LoadBookmarks();
      
      OpenNewBrowser();

      Application.Run();
    }

    public void Quit() 
    {
      // Flush preferences to disk.
      mPreferences.FlushUserPreferences();
      
      Application.Exit();
    }

    public void WindowClosed(BrowserWindow aWindow) 
    {
      if (mBrowserWindows.ContainsKey(aWindow.GetHashCode()))
        mBrowserWindows.Remove(aWindow.GetHashCode());
      
      // When window count drops to zero, quit. 
      // XXX - a little hacky for now, will eventually reflect
      //       all windows. 
      if (mBrowserWindows.Count == 0)
        Quit();
    }

    // Opens and displays a new browser window
    public BrowserWindow OpenNewBrowser()
    {
      BrowserWindow window = new BrowserWindow(this);
      mBrowserWindows.Add(window.GetHashCode(), window);
      window.Show();
      return window;
    }

    public static void Main(string[] args) 
    {
      ManticoreApp app = new ManticoreApp();
    }
  }
}

