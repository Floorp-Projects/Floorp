using System;
using System.Collections;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Windows.Forms;

namespace Silverstone.Manticore.Browser
{
	/// <summary>
	/// Summary description for BrowserDisplayPanel.
	/// </summary>
	public class BrowserDisplayPanel : PrefPanel
	{
    private GroupBox groupBox1;
    private Button button2;
    private Label label2;
    private Button button1;
    private TextBox textBox1;
    private Label label1;
    private GroupBox groupBox2;
    private Button restoreSessionSettingsButton;
    private RadioButton radioButton3;
    private RadioButton radioButton2;
    private RadioButton radioButton1;
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public BrowserDisplayPanel()
		{
			// This call is required by the Windows.Forms Form Designer.
			InitializeComponent();

      Console.WriteLine("Done initializeing browser display panel");
    }

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		#region Component Designer generated code
		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.groupBox1 = new System.Windows.Forms.GroupBox();
      this.button2 = new System.Windows.Forms.Button();
      this.label2 = new System.Windows.Forms.Label();
      this.button1 = new System.Windows.Forms.Button();
      this.textBox1 = new System.Windows.Forms.TextBox();
      this.label1 = new System.Windows.Forms.Label();
      this.groupBox2 = new System.Windows.Forms.GroupBox();
      this.restoreSessionSettingsButton = new System.Windows.Forms.Button();
      this.radioButton3 = new System.Windows.Forms.RadioButton();
      this.radioButton2 = new System.Windows.Forms.RadioButton();
      this.radioButton1 = new System.Windows.Forms.RadioButton();
      this.groupBox1.SuspendLayout();
      this.groupBox2.SuspendLayout();
      this.SuspendLayout();
      // 
      // groupBox1
      // 
      this.groupBox1.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                            this.button2,
                                                                            this.label2,
                                                                            this.button1,
                                                                            this.textBox1,
                                                                            this.label1});
      this.groupBox1.Location = new System.Drawing.Point(8, 112);
      this.groupBox1.Name = "groupBox1";
      this.groupBox1.Size = new System.Drawing.Size(312, 104);
      this.groupBox1.TabIndex = 0;
      this.groupBox1.TabStop = false;
      this.groupBox1.Text = "Home Page";
      // 
      // button2
      // 
      this.button2.Location = new System.Drawing.Point(136, 72);
      this.button2.Name = "button2";
      this.button2.TabIndex = 4;
      this.button2.Text = "Use Current";
      // 
      // label2
      // 
      this.label2.Location = new System.Drawing.Point(8, 48);
      this.label2.Name = "label2";
      this.label2.Size = new System.Drawing.Size(56, 16);
      this.label2.TabIndex = 3;
      this.label2.Text = "Location:";
      // 
      // button1
      // 
      this.button1.Location = new System.Drawing.Point(221, 72);
      this.button1.Name = "button1";
      this.button1.Size = new System.Drawing.Size(75, 24);
      this.button1.TabIndex = 2;
      this.button1.Text = "Browse...";
      // 
      // textBox1
      // 
      this.textBox1.Location = new System.Drawing.Point(64, 48);
      this.textBox1.Name = "textBox1";
      this.textBox1.Size = new System.Drawing.Size(232, 20);
      this.textBox1.TabIndex = 1;
      this.textBox1.Text = "http://www.silverstone.net.nz/";
      // 
      // label1
      // 
      this.label1.Location = new System.Drawing.Point(8, 24);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(280, 16);
      this.label1.TabIndex = 0;
      this.label1.Text = "Clicking the Home button takes you to this page:";
      // 
      // groupBox2
      // 
      this.groupBox2.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                            this.restoreSessionSettingsButton,
                                                                            this.radioButton3,
                                                                            this.radioButton2,
                                                                            this.radioButton1});
      this.groupBox2.Location = new System.Drawing.Point(8, 0);
      this.groupBox2.Name = "groupBox2";
      this.groupBox2.Size = new System.Drawing.Size(312, 104);
      this.groupBox2.TabIndex = 1;
      this.groupBox2.TabStop = false;
      this.groupBox2.Text = "When Manticore starts, ";
      // 
      // restoreSessionSettingsButton
      // 
      this.restoreSessionSettingsButton.Location = new System.Drawing.Point(224, 72);
      this.restoreSessionSettingsButton.Name = "restoreSessionSettingsButton";
      this.restoreSessionSettingsButton.TabIndex = 3;
      this.restoreSessionSettingsButton.Text = "Settings...";
      // 
      // radioButton3
      // 
      this.radioButton3.Location = new System.Drawing.Point(16, 72);
      this.radioButton3.Name = "radioButton3";
      this.radioButton3.Size = new System.Drawing.Size(152, 16);
      this.radioButton3.TabIndex = 2;
      this.radioButton3.Text = "Restore previous session";
      // 
      // radioButton2
      // 
      this.radioButton2.Location = new System.Drawing.Point(16, 48);
      this.radioButton2.Name = "radioButton2";
      this.radioButton2.Size = new System.Drawing.Size(112, 16);
      this.radioButton2.TabIndex = 1;
      this.radioButton2.Text = "Show blank page";
      // 
      // radioButton1
      // 
      this.radioButton1.Location = new System.Drawing.Point(16, 24);
      this.radioButton1.Name = "radioButton1";
      this.radioButton1.Size = new System.Drawing.Size(120, 16);
      this.radioButton1.TabIndex = 0;
      this.radioButton1.Text = "Show home page";
      // 
      // BrowserDisplayPanel
      // 
      this.Controls.AddRange(new System.Windows.Forms.Control[] { this.groupBox1,
                                                                  this.groupBox2});
      this.Name = "BrowserDisplayPanel";
      this.Size = new System.Drawing.Size(320, 280);
      this.groupBox1.ResumeLayout(false);
      this.groupBox2.ResumeLayout(false);
      this.ResumeLayout(false);

    }
		#endregion
	}
}
