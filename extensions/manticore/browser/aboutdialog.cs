
namespace Silverstone.Manticore.AboutDialog
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  public class AboutDialog : System.Windows.Forms.Form 
  {
    private System.ComponentModel.Container components;

    private Form parentWindow;

    public AboutDialog(Form parent)
    {
      parentWindow = parent;

      InitializeComponent();

      // Show the about dialog modally.
      this.ShowDialog();

    }

    public override void Dispose()
    {
      base.Dispose();
      components.Dispose();
    }

    private void InitializeComponent()
    {
      this.components = new System.ComponentModel.Container();

      this.Width = 253;
      this.Height = 195;

      this.Left = parentWindow.Left + ((parentWindow.Width / 2) - (this.Width / 2));
      this.Top = parentWindow.Top + ((parentWindow.Height / 2) - (this.Height / 2));
      
      // Borderless dialog
      this.FormBorderStyle = FormBorderStyle.None;

      this.BackgroundImage = Image.FromFile("manticore.png");
      
      this.Text = "About Manticore"; // XXX localize

      this.Click += new EventHandler(this.CloseAboutDialog);
    }

    public void CloseAboutDialog(Object sender, EventArgs e)
    {
      this.Close();
    }
  }
}


