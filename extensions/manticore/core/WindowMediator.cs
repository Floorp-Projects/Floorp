
namespace Silverstone.Manticore.Core
{
  using System;
  using System.Collections;

  using Silverstone.Manticore.App;
  using Silverstone.Manticore.Toolkit;
  using Silverstone.Manticore.Browser;

  // XXX - TODO: need to add logic to quit application when there are 
  //             no more windows

  /// <summary>
	/// Summary description for WindowMediator.
	/// </summary>
	public class WindowMediator
	{
		public WindowMediator()
		{
      mWindows = new Hashtable();
      mRecentWindows = new Hashtable();
		}

    /// <summary>
    /// 
    /// </summary>
    /// <returns></returns>
    public IEnumerator GetEnumerator()
    {
      return mWindows.GetEnumerator();
    }

    public IEnumerator GetEnumeratorForType(String aType)
    {
      return (mWindows[aType] as Hashtable).GetEnumerator();
    }

    protected Hashtable mWindows;
    protected Hashtable mRecentWindows;

    public ManticoreWindow GetMostRecentWindow(String aType)
    {
      if (mRecentWindows.ContainsKey(aType)) 
        return mRecentWindows[aType] as ManticoreWindow;
      return null;
    }

    public void SetMostRecentWindow(ManticoreWindow aWindow)
    {
      if (!mRecentWindows.ContainsKey(aWindow.Type))
        mRecentWindows.Add(aWindow.Type, aWindow);
      else
        mRecentWindows[aWindow.Type] = aWindow;
    }

    public void RegisterWindow(ManticoreWindow aWindow)
    {
      if (!mWindows.ContainsKey(aWindow.Type))
        mWindows[aWindow.Type] = new Hashtable();
      Hashtable windowList = mWindows[aWindow.Type] as Hashtable;
      windowList.Add(aWindow.GetHashCode(), aWindow);
    }

    public void UnregisterWindow(ManticoreWindow aWindow)
    {
      mWindows.Remove(aWindow.GetHashCode());
      
      // If this is the last window of a specific type, remove it from the window list
      Hashtable windowsForType = mWindows[aWindow.Type] as Hashtable;
      IEnumerator e = windowsForType.GetEnumerator();
      e.MoveNext();
      ManticoreWindow window = e.Current as ManticoreWindow;
      if (window == null)
        mWindows.Remove(aWindow.Type);
      
      ManticoreWindow mostRecentWindow = GetMostRecentWindow(aWindow.Type);
      if (mostRecentWindow == window) 
      {
        if (window != null) 
          SetMostRecentWindow(window);
        else
          mRecentWindows.Remove(aWindow.Type);
      }
    }
  }
}
