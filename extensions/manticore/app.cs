/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Silverstone Interactive. Portions created by Silverstone Interactive are
 * Copyright (C) 2001 Silverstone Interactive. 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Ben Goodger <ben@netscape.com>
 *
 */

namespace Silverstone.Manticore.App 
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;
  using System.Collections;

  using Silverstone.Manticore.Browser;
  using Silverstone.Manticore.Core;
  
  public class ManticoreApp
  {
    public ManticoreApp()
    {
      if (!RestoreSession()) 
        BrowserWindow.OpenBrowser();

      Application.Run();
    }

    public void Quit() 
    {
      // Save Session
      SaveSession();
      
      // Flush preferences to disk.
      ServiceManager.Preferences.FlushUserPreferences();
      
      Application.Exit();
    }

    /// <summary>
    /// Saves the state of opened windows, urls, etc to preferences
    /// so that state can be restored when the app is restarted.
    /// </summary>
    private void SaveSession()
    {
      if (ServiceManager.Preferences.GetIntPref("browser.homepage.mode") == 2) {
        bool isLastPageOnly = ServiceManager.Preferences.GetIntPref("browser.session.windowmode") == 0;
        WindowMediator wm = ServiceManager.WindowMediator;
        IEnumerator browsers = wm.GetEnumeratorForType("BrowserWindow");
        int count = 0;
        while (browsers.MoveNext()) {
          if (isLastPageOnly && count > 0) {
            // XXX need to |getMostRecentWindow| instead of just taking the first. 
            break;
          }

          BrowserWindow currWindow = browsers.Current as BrowserWindow;
          String pref = "browser.session.windows.";
          pref += currWindow.Type + count++;
          ServiceManager.Preferences.SetStringPref(pref, currWindow.URL);
          ServiceManager.Preferences.SetIntPref(pref + ".left", currWindow.Left);
          ServiceManager.Preferences.SetIntPref(pref + ".top", currWindow.Top);
          ServiceManager.Preferences.SetIntPref(pref + ".width", currWindow.Width);
          ServiceManager.Preferences.SetIntPref(pref + ".height", currWindow.Height);
        }
        // XXX need to save session histories. 
      }
    }

    /// <summary>
    /// Restore Session from preferences if "Restore Session" pref is set. 
    /// </summary>
    /// <returns>Whether or not a session was restored.</returns>
    private bool RestoreSession()
    {
      if (ServiceManager.Preferences.GetIntPref("browser.homepage.mode") == 2) {
        IEnumerator branch = ServiceManager.Preferences.GetBranch("browser.session.windows");
        while (branch.MoveNext()) {
          String pref = branch.Current as String;
          String url = ServiceManager.Preferences.GetStringPref(pref);
          int x = ServiceManager.Preferences.GetIntPref(pref + ".left");
          int y = ServiceManager.Preferences.GetIntPref(pref + ".top");
          int width = ServiceManager.Preferences.GetIntPref(pref + ".width");
          int height = ServiceManager.Preferences.GetIntPref(pref + ".height");
       
          // Create a new browser with the applicable url at the applicable
          // location. 
          BrowserWindow window = BrowserWindow.OpenBrowserWithURL(url);
          window.Location = new Point(x, y);
          window.Size = new Size(width, height);
        }
        // XXX need to reinit session histories. 
        ServiceManager.Preferences.RemoveBranch("browser.session.windows");
        return true;
      }
      return false;
    }

    [STAThread]
    public static void Main(string[] args) 
    {
      ServiceManager.mApp = new ManticoreApp();
    }
  }
}

