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
	/// Summary description for RestoreSessionSettings.
	/// </summary>
	public class RestoreSessionSettings : ManticoreDialog
	{
    private System.Windows.Forms.Label label1;
    private System.Windows.Forms.CheckBox checkBox1;
    private System.Windows.Forms.CheckBox checkBox2;
    private System.Windows.Forms.CheckBox checkBox3;
    private System.Windows.Forms.Button okButton;
    private System.Windows.Forms.Button cancelButton;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public RestoreSessionSettings(Form aOpener) : base(aOpener)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
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
      this.checkBox3 = new System.Windows.Forms.CheckBox();
      this.checkBox2 = new System.Windows.Forms.CheckBox();
      this.cancelButton = new System.Windows.Forms.Button();
      this.label1 = new System.Windows.Forms.Label();
      this.okButton = new System.Windows.Forms.Button();
      this.checkBox1 = new System.Windows.Forms.CheckBox();
      this.SuspendLayout();
      // 
      // checkBox3
      // 
      this.checkBox3.Location = new System.Drawing.Point(32, 120);
      this.checkBox3.Name = "checkBox3";
      this.checkBox3.Size = new System.Drawing.Size(136, 24);
      this.checkBox3.TabIndex = 3;
      this.checkBox3.Text = "Recent pages history";
      // 
      // checkBox2
      // 
      this.checkBox2.Location = new System.Drawing.Point(32, 96);
      this.checkBox2.Name = "checkBox2";
      this.checkBox2.Size = new System.Drawing.Size(136, 24);
      this.checkBox2.TabIndex = 2;
      this.checkBox2.Text = "Open windows";
      // 
      // cancelButton
      // 
      this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.cancelButton.Location = new System.Drawing.Point(176, 160);
      this.cancelButton.Name = "cancelButton";
      this.cancelButton.TabIndex = 5;
      this.cancelButton.Text = "Cancel";
      // 
      // label1
      // 
      this.label1.Location = new System.Drawing.Point(16, 16);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(232, 48);
      this.label1.TabIndex = 0;
      this.label1.Text = "When you start Manticore, the following items from your previous browsing session" +
        " are restored.";
      // 
      // okButton
      // 
      this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.okButton.Location = new System.Drawing.Point(96, 160);
      this.okButton.Name = "okButton";
      this.okButton.TabIndex = 4;
      this.okButton.Text = "OK";
      // 
      // checkBox1
      // 
      this.checkBox1.Location = new System.Drawing.Point(32, 72);
      this.checkBox1.Name = "checkBox1";
      this.checkBox1.Size = new System.Drawing.Size(136, 24);
      this.checkBox1.TabIndex = 1;
      this.checkBox1.Text = "Last page(s) visited";
      // 
      // RestoreSessionSettings
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(264, 192);
      this.ControlBox = false;
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.cancelButton,
                                                                  this.okButton,
                                                                  this.checkBox3,
                                                                  this.checkBox2,
                                                                  this.checkBox1,
                                                                  this.label1});
      this.Name = "RestoreSessionSettings";
      this.ShowInTaskbar = false;
      this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
      this.Text = "Restore Session";
      this.Load += new System.EventHandler(this.RestoreSessionSettings_Load);
      this.ResumeLayout(false);

    }
		#endregion

    private void RestoreSessionSettings_Load(object sender, System.EventArgs e)
    {

    }
	}
}
