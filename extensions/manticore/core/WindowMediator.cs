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
 *   Ben Goodger <ben@netscape.com> (Original Author)
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
