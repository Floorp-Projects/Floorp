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
  using System.Drawing;
  using System.Collections;
  using System.ComponentModel;
  using System.Windows.Forms;
  
  using Silverstone.Manticore.Toolkit;

	/// <summary>
	/// Summary description for Form1.
	/// </summary>
	public class PrefsDialog : ManticoreDialog
	{
    private System.Windows.Forms.TreeView treeView1;
    private System.Windows.Forms.Panel panel1;
    private System.Windows.Forms.Button cancelButton;
    private System.Windows.Forms.Button okButton;
    private System.Windows.Forms.GroupBox groupBox1;
    private System.Windows.Forms.Label label1;
    private System.Windows.Forms.TextBox textBox1;
    private System.Windows.Forms.Button button1;
    private System.Windows.Forms.Label label2;
    private System.Windows.Forms.Button button2;
    private System.Windows.Forms.GroupBox groupBox2;
    private System.Windows.Forms.RadioButton radioButton1;
    private System.Windows.Forms.RadioButton radioButton2;
    private System.Windows.Forms.RadioButton radioButton3;
    private System.Windows.Forms.Button restoreSessionSettingsButton;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public PrefsDialog(Form aOpener) : base(aOpener)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      //
      // Initialize the category list. 
      //
      InitializeCategoryList();
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

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.button1 = new System.Windows.Forms.Button();
      this.button2 = new System.Windows.Forms.Button();
      this.radioButton3 = new System.Windows.Forms.RadioButton();
      this.restoreSessionSettingsButton = new System.Windows.Forms.Button();
      this.cancelButton = new System.Windows.Forms.Button();
      this.radioButton1 = new System.Windows.Forms.RadioButton();
      this.treeView1 = new System.Windows.Forms.TreeView();
      this.radioButton2 = new System.Windows.Forms.RadioButton();
      this.panel1 = new System.Windows.Forms.Panel();
      this.groupBox2 = new System.Windows.Forms.GroupBox();
      this.groupBox1 = new System.Windows.Forms.GroupBox();
      this.label2 = new System.Windows.Forms.Label();
      this.textBox1 = new System.Windows.Forms.TextBox();
      this.label1 = new System.Windows.Forms.Label();
      this.okButton = new System.Windows.Forms.Button();
      this.panel1.SuspendLayout();
      this.groupBox2.SuspendLayout();
      this.groupBox1.SuspendLayout();
      this.SuspendLayout();
      // 
      // button1
      // 
      this.button1.Location = new System.Drawing.Point(221, 72);
      this.button1.Name = "button1";
      this.button1.Size = new System.Drawing.Size(75, 24);
      this.button1.TabIndex = 2;
      this.button1.Text = "Browse...";
      // 
      // button2
      // 
      this.button2.Location = new System.Drawing.Point(136, 72);
      this.button2.Name = "button2";
      this.button2.TabIndex = 4;
      this.button2.Text = "Use Current";
      // 
      // radioButton3
      // 
      this.radioButton3.Location = new System.Drawing.Point(16, 72);
      this.radioButton3.Name = "radioButton3";
      this.radioButton3.Size = new System.Drawing.Size(152, 16);
      this.radioButton3.TabIndex = 2;
      this.radioButton3.Text = "Restore previous session";
      // 
      // restoreSessionSettingsButton
      // 
      this.restoreSessionSettingsButton.Location = new System.Drawing.Point(224, 72);
      this.restoreSessionSettingsButton.Name = "restoreSessionSettingsButton";
      this.restoreSessionSettingsButton.TabIndex = 3;
      this.restoreSessionSettingsButton.Text = "Settings...";
      this.restoreSessionSettingsButton.Click += new System.EventHandler(this.restoreSessionSettingsButton_Click);
      // 
      // cancelButton
      // 
      this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.cancelButton.Location = new System.Drawing.Point(408, 296);
      this.cancelButton.Name = "cancelButton";
      this.cancelButton.TabIndex = 2;
      this.cancelButton.Text = "Cancel";
      // 
      // radioButton1
      // 
      this.radioButton1.Location = new System.Drawing.Point(16, 24);
      this.radioButton1.Name = "radioButton1";
      this.radioButton1.Size = new System.Drawing.Size(120, 16);
      this.radioButton1.TabIndex = 0;
      this.radioButton1.Text = "Show home page";
      // 
      // treeView1
      // 
      this.treeView1.ImageIndex = -1;
      this.treeView1.Location = new System.Drawing.Point(16, 16);
      this.treeView1.Name = "treeView1";
      this.treeView1.SelectedImageIndex = -1;
      this.treeView1.Size = new System.Drawing.Size(136, 264);
      this.treeView1.TabIndex = 0;
      // 
      // radioButton2
      // 
      this.radioButton2.Location = new System.Drawing.Point(16, 48);
      this.radioButton2.Name = "radioButton2";
      this.radioButton2.Size = new System.Drawing.Size(112, 16);
      this.radioButton2.TabIndex = 1;
      this.radioButton2.Text = "Show blank page";
      // 
      // panel1
      // 
      this.panel1.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                         this.groupBox2,
                                                                         this.groupBox1});
      this.panel1.Location = new System.Drawing.Point(160, 16);
      this.panel1.Name = "panel1";
      this.panel1.Size = new System.Drawing.Size(320, 264);
      this.panel1.TabIndex = 1;
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
      // label2
      // 
      this.label2.Location = new System.Drawing.Point(8, 48);
      this.label2.Name = "label2";
      this.label2.Size = new System.Drawing.Size(56, 16);
      this.label2.TabIndex = 3;
      this.label2.Text = "Location:";
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
      // okButton
      // 
      this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.okButton.Location = new System.Drawing.Point(328, 296);
      this.okButton.Name = "okButton";
      this.okButton.TabIndex = 3;
      this.okButton.Text = "OK";
      // 
      // PrefsDialog
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(496, 328);
      this.ControlBox = false;
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.okButton,
                                                                  this.cancelButton,
                                                                  this.panel1,
                                                                  this.treeView1});
      this.HelpButton = true;
      this.Name = "PrefsDialog";
      this.ShowInTaskbar = false;
      this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
      this.Text = "Options";
      this.Load += new System.EventHandler(this.PrefsDialog_Load);
      this.panel1.ResumeLayout(false);
      this.groupBox2.ResumeLayout(false);
      this.groupBox1.ResumeLayout(false);
      this.ResumeLayout(false);

    }
		#endregion

    private void restoreSessionSettingsButton_Click(object sender, System.EventArgs e)
    {
      RestoreSessionSettings dlg = new RestoreSessionSettings(this);
      dlg.ShowDialog();
    }

    private void PrefsDialog_Load(object sender, System.EventArgs e)
    {
      
    }
	}
}
