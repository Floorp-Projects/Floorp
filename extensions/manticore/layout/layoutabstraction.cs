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

namespace Silverstone.Manticore.LayoutAbstraction
{
  using System;
  using Microsoft.Win32;
  using System.Drawing;
  using System.ComponentModel;
  using System.Windows.Forms;
  using System.Runtime.InteropServices;

  // Trident
  using AxSHDocVw;
  using MSHTML;
  
  // Gecko
  using AxMOZILLACONTROLLib;
  using MOZILLACONTROLLib;

  public class WebBrowser : ContainerControl
  {
    private AxWebBrowser trident;
    private AxMozillaBrowser gecko;

    public WebBrowser()
    {
      this.Dock = DockStyle.Fill;
    }

    public void RealizeLayoutEngine()
    {
      if (gecko == null && trident == null)
        SwitchLayoutEngine("gecko"); // XXX Should be pref-based.
    }

    public void SwitchLayoutEngine(String id)
    {
      AxHost host = null;
      switch (id) {
      case "trident":
        if (gecko != null) {
          this.Controls.Remove(gecko as AxHost);
          gecko = null;
        }

        if (trident == null) {
          trident = new AxWebBrowser();
          host = trident as AxHost;
        }

        break;
      default:
        if (trident != null) {
          this.Controls.Remove(trident as AxHost);
          trident = null;
        }

        if (gecko == null) {
          gecko = new AxMozillaBrowser(); 
          host = gecko as AxHost;
        }
        break;
      }

      if (host != null) {
        host.BeginInit();
        host.Size = new Size(600, 200);
        host.TabIndex = 1;
        host.Dock = DockStyle.Fill;
        host.EndInit();
        this.Controls.Add(host);
      }
    }

    public void LoadURL(String url, Boolean bypassCache)
    {
      // XXX - neither IE nor Mozilla implement all of the
      //       load flags properly. Mozilla should at least be 
      //       made to support ignore-cache and ignore-history.
      //       This will require modification to the ActiveX
      //       control.
      RealizeLayoutEngine();
      Object o = null;
      if (gecko != null)
        gecko.Navigate(url, ref o, ref o, ref o, ref o);
      else if (trident != null)
        trident.Navigate(url, ref o, ref o, ref o, ref o);
    }

    public void GoHome()
    {
      // XXX - need to implement "Home" preference
      LoadURL("http://www.mozilla.org/", false);
    }
  }
  

}