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
    private IWebBrowser2 browser;

    public WebBrowser()
    {
      InitializeComponent();
    }

    private void InitializeComponent()
    {
      // XXX - remember this setting in a pref
      SwitchLayoutEngine("gecko");

      this.Dock = DockStyle.Fill;
    }

    public void SwitchLayoutEngine(String id)
    {
      AxHost host;
      switch (id) {
      case "trident":
        host = new AxWebBrowser() as AxHost;
        break;
      default:
        host = new AxMozillaBrowser() as AxHost; 
        break;
      }

      host.BeginInit();
      host.Size = new Size(600, 200);
      host.TabIndex = 1;
      host.Dock = DockStyle.Fill;
      host.EndInit();
      this.Controls.Add(host);

      browser = host as IWebBrowser2;
    }

    public void LoadURL(String url, Boolean bypassCache)
    {
      // XXX - neither IE nor Mozilla implement all of the
      //       load flags properly. Mozilla should at least be 
      //       made to support ignore-cache and ignore-history.
      //       This will require modification to the ActiveX
      //       control.
      Object o = null;
//      browser.Navigate(url, ref o, ref o, ref o, ref o);
    }

    public void GoHome()
    {
      // XXX - need to implement "Home" preference
      browser.GoHome();
    }
  }
  

}