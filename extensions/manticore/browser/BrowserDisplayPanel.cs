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

namespace Silverstone.Manticore.Browser
{
  using System;
  using System.Collections;
  using System.ComponentModel;
  using System.Drawing;
  using System.Data;
  using System.Windows.Forms;

  using Silverstone.Manticore.Core;

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

    /// <summary>
    /// LAME - the window that invoked the preferences dialog
    /// </summary>
    private Form mOpener = null;

    private int mWindowOpenMode = 1;
    private bool mSaveSessionHistory = false;

    /// <summary>
    /// Construct a BrowserDisplayPanel
    /// </summary>
    /// <param name="aParent"></param>
    /// <param name="aOpener">
    /// The window that opened the preferencs dialog. We'd really rather get rid of
    /// this and replace it with calls to a window-mediator service.
    /// </param>
    /// <param name="aPrefs">
    /// The preferences handle from the application object. We want to get rid of this
    /// and use some kind of global preferences service. 
    /// </param>
		public BrowserDisplayPanel(Form aParent, Form aOpener, Preferences aPrefs) : base(aParent, aPrefs)
		{
			// This call is required by the Windows.Forms Form Designer.
			InitializeComponent();

      mOpener = aOpener;

      restoreSessionSettingsButton.Enabled = false;

      radioButton3.Click += new EventHandler(OnStartupModeRadio);
      radioButton2.Click += new EventHandler(OnStartupModeRadio);
      radioButton1.Click += new EventHandler(OnStartupModeRadio);

      restoreSessionSettingsButton.Click += new EventHandler(OnRestoreSessionSettings);

      button2.Click += new EventHandler(OnUseCurrent);
      button1.Click += new EventHandler(OnBrowseForHomepage);
    }

    protected internal void OnUseCurrent(object sender, EventArgs e)
    {
      // XXX what we really want to do here is use a window-mediator like
      //     entity to find the most recent browser window. This will surely
      //     fail miserably if the opening window isn't a browser. 
      BrowserWindow window = mOpener as BrowserWindow;
      if (window != null)
        textBox1.Text = window.URL;
    }

    protected internal void OnBrowseForHomepage(object sender, EventArgs e)
    {
      OpenFileDialog ofd = new OpenFileDialog();
      // Initially set directory to "My Documents";
      ofd.InitialDirectory = FileLocator.GetFolderPath(FileLocator.SpecialFolders.ssfPERSONAL);
      ofd.Filter = "HTML documents (*.html;*.htm)|*.html;*.htm|All files (*.*)|*.*";
      ofd.FilterIndex = 1;
      ofd.RestoreDirectory = true;
      ofd.Title = "Browse for Home Page";

      if(ofd.ShowDialog() == DialogResult.OK)
        textBox1.Text = ofd.FileName;
    }

    public void OnRestoreSessionSettings(object sender, EventArgs e)
    {
      RestoreSessionSettings dlg = new RestoreSessionSettings(mParent);
      dlg.WindowOpenMode = mWindowOpenMode;
      dlg.SaveSessionHistory = mSaveSessionHistory;
      if (dlg.ShowDialog() == DialogResult.OK) {
        mWindowOpenMode = dlg.WindowOpenMode;
        mSaveSessionHistory = dlg.SaveSessionHistory;
      }
    }

    public void OnStartupModeRadio(object sender, EventArgs e)
    {
      // Enable the Restore Session Settings button when the Restore Settings
      // startup mode is enabled. 
      restoreSessionSettingsButton.Enabled = radioButton3.Checked;
    }

    public override void Load()
    {
      mWindowOpenMode = mPrefs.GetIntPref("browser.session.windowmode");
      mSaveSessionHistory = mPrefs.GetBoolPref("browser.session.savesessionhistory");

      String homepageURL = mPrefs.GetStringPref("browser.homepage");
      textBox1.Text = homepageURL;

      int startMode = mPrefs.GetIntPref("browser.homepage.mode");
      switch (startMode) {
        case 2:
          radioButton3.Checked = true;
          restoreSessionSettingsButton.Enabled = true;
          break;
        case 0:
          radioButton2.Checked = true;
          break;
        case 1:
        default:
          radioButton1.Checked = true;
          break;
      }
    }

    public override void Save()
    {
      mPrefs.SetStringPref("browser.homepage", textBox1.Text != "" ? textBox1.Text : "about:blank");

      int mode = 1;
      if (radioButton3.Checked) 
        mode = 2;
      else if (radioButton2.Checked)
        mode = 0;
      mPrefs.SetIntPref("browser.homepage.mode", mode);

      mPrefs.SetIntPref("browser.session.windowmode", mWindowOpenMode);
      mPrefs.SetBoolPref("browser.session.savesessionhistory", mSaveSessionHistory);
    }

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
      if (disposing && components != null)
        components.Dispose();
      base.Dispose(disposing);
		}

		#region Component Designer generated code
		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.groupBox1 = new GroupBox();
      this.button2 = new Button();
      this.label2 = new Label();
      this.button1 = new Button();
      this.textBox1 = new TextBox();
      this.label1 = new Label();
      this.groupBox2 = new GroupBox();
      this.restoreSessionSettingsButton = new Button();
      this.radioButton3 = new RadioButton();
      this.radioButton2 = new RadioButton();
      this.radioButton1 = new RadioButton();
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
      this.button2.FlatStyle = FlatStyle.System;
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
      this.button2.FlatStyle = FlatStyle.System;
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
      this.groupBox2.Name = "startupModeGroup";
      this.groupBox2.Size = new System.Drawing.Size(312, 104);
      this.groupBox2.TabIndex = 1;
      this.groupBox2.TabStop = false;
      this.groupBox2.Text = "When Manticore starts, ";
      // 
      // restoreSessionSettingsButton
      // 
      this.restoreSessionSettingsButton.Location = new Point(224, 72);
      this.restoreSessionSettingsButton.Name = "restoreSessionSettingsButton";
      this.restoreSessionSettingsButton.TabIndex = 3;
      this.restoreSessionSettingsButton.Text = "Settings...";
      this.restoreSessionSettingsButton.FlatStyle = FlatStyle.System;
      // 
      // radioButton3
      // 
      this.radioButton3.Location = new System.Drawing.Point(16, 72);
      this.radioButton3.Name = "restoreSessionRadio";
      this.radioButton3.Size = new System.Drawing.Size(152, 16);
      this.radioButton3.TabIndex = 2;
      this.radioButton3.Text = "Restore previous session";
      this.radioButton3.FlatStyle = FlatStyle.System;
      // 
      // radioButton2
      // 
      this.radioButton2.Location = new System.Drawing.Point(16, 48);
      this.radioButton2.Name = "blankPageRadio";
      this.radioButton2.Size = new System.Drawing.Size(112, 16);
      this.radioButton2.TabIndex = 1;
      this.radioButton2.Text = "Show blank page";
      this.radioButton2.FlatStyle = FlatStyle.System;
      // 
      // radioButton1
      // 
      this.radioButton1.Location = new System.Drawing.Point(16, 24);
      this.radioButton1.Name = "homePageRadio";
      this.radioButton1.Size = new System.Drawing.Size(120, 16);
      this.radioButton1.TabIndex = 0;
      this.radioButton1.Text = "Show home page";
      this.radioButton1.FlatStyle = FlatStyle.System;
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
