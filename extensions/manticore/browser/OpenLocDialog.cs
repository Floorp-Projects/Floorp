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
 *  David Hyatt <hyatt@netscape.com>
 *
 */

namespace Silverstone.Manticore.Browser
{
  using System;
  using System.Drawing;
  using System.Collections;
  using System.ComponentModel;
  using System.Windows.Forms;
  
  /// <summary>
	/// Summary description for OpenDialog.
	/// </summary>
	public class OpenDialog : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox urlText;
		private System.Windows.Forms.Button openButton;
		private System.Windows.Forms.Button cancelButton;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
 
		public OpenDialog()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			openButton.DialogResult = DialogResult.OK;
		}

		public String URL 
		{
		  get {
		    return urlText.Text;
		  }
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
			this.urlText = new System.Windows.Forms.TextBox();
			this.cancelButton = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.openButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// urlText
			// 
			this.urlText.Location = new System.Drawing.Point(16, 32);
			this.urlText.Name = "urlText";
			this.urlText.Size = new System.Drawing.Size(272, 20);
			this.urlText.TabIndex = 1;
			this.urlText.Text = "";
			// 
			// cancelButton
			// 
			this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.cancelButton.Location = new System.Drawing.Point(216, 64);
			this.cancelButton.Name = "cancelButton";
			this.cancelButton.Size = new System.Drawing.Size(72, 24);
			this.cancelButton.TabIndex = 2;
			this.cancelButton.Text = "Cancel";
      this.cancelButton.FlatStyle = FlatStyle.System;
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(16, 8);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(264, 24);
			this.label1.TabIndex = 0;
			this.label1.Text = "Enter a URL:";
			// 
			// openButton
			// 
			this.openButton.Location = new System.Drawing.Point(136, 64);
			this.openButton.Name = "openButton";
			this.openButton.Size = new System.Drawing.Size(72, 24);
			this.openButton.TabIndex = 2;
			this.openButton.Text = "Open";
      this.openButton.FlatStyle = FlatStyle.System;
			// 
			// OpenDialog
			// 
			this.AcceptButton = this.openButton;
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.CancelButton = this.cancelButton;
			this.ClientSize = new System.Drawing.Size(302, 98);
			this.ControlBox = false;
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.cancelButton,
																		  this.openButton,
																		  this.urlText,
																		  this.label1});
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "OpenDialog";
			this.ShowInTaskbar = false;
			this.Text = "Open Location";
			this.ResumeLayout(false);

		}
		#endregion
	}
}
