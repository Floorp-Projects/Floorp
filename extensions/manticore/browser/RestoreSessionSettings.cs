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
 *   Ben Goodger <ben@netscape.com>
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
    private System.Windows.Forms.CheckBox checkBox3;
    private System.Windows.Forms.Button okButton;
    private System.Windows.Forms.Button cancelButton;
    private System.Windows.Forms.RadioButton radioButton1;
    private System.Windows.Forms.RadioButton radioButton2;
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
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if (disposing && components != null)
        components.Dispose();
			base.Dispose( disposing );
		}
    
    public int WindowOpenMode
    {
      get {
        return radioButton1.Checked ? 0 : 1;
      }
      set {
        if (value == 0) 
          radioButton1.Checked = true;
        else 
          radioButton2.Checked = true;
      }
    }

    public bool SaveSessionHistory
    {
      get {
        return checkBox3.Checked;
      }
      set {
        checkBox3.Checked = value;
      }
    }


		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.checkBox3 = new System.Windows.Forms.CheckBox();
      this.cancelButton = new System.Windows.Forms.Button();
      this.label1 = new System.Windows.Forms.Label();
      this.okButton = new System.Windows.Forms.Button();
      this.radioButton1 = new System.Windows.Forms.RadioButton();
      this.radioButton2 = new System.Windows.Forms.RadioButton();
      this.SuspendLayout();
      // 
      // checkBox3
      // 
      this.checkBox3.Location = new System.Drawing.Point(32, 120);
      this.checkBox3.Name = "checkBox3";
      this.checkBox3.Size = new System.Drawing.Size(136, 24);
      this.checkBox3.TabIndex = 3;
      this.checkBox3.Text = "Recent pages history";
      this.checkBox3.FlatStyle = FlatStyle.System;
      // 
      // cancelButton
      // 
      this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.cancelButton.Location = new System.Drawing.Point(176, 160);
      this.cancelButton.Name = "cancelButton";
      this.cancelButton.TabIndex = 5;
      this.cancelButton.Text = "Cancel";
      this.cancelButton.FlatStyle = FlatStyle.System;
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
      this.okButton.FlatStyle = FlatStyle.System;
      // 
      // radioButton1
      // 
      this.radioButton1.Location = new System.Drawing.Point(32, 72);
      this.radioButton1.Name = "radioButton1";
      this.radioButton1.Size = new System.Drawing.Size(144, 16);
      this.radioButton1.TabIndex = 6;
      this.radioButton1.Text = "Last page visited";
      this.radioButton1.FlatStyle = FlatStyle.System;
      // 
      // radioButton2
      // 
      this.radioButton2.Location = new System.Drawing.Point(32, 96);
      this.radioButton2.Name = "radioButton2";
      this.radioButton2.Size = new System.Drawing.Size(104, 16);
      this.radioButton2.TabIndex = 7;
      this.radioButton2.Text = "Open windows";
      this.radioButton2.FlatStyle = FlatStyle.System;
      // 
      // RestoreSessionSettings
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(264, 192);
      this.ControlBox = false;
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.radioButton2,
                                                                  this.radioButton1,
                                                                  this.cancelButton,
                                                                  this.okButton,
                                                                  this.checkBox3,
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
