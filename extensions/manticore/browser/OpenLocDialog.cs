using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace Silverstone.Manticore.OpenDialog
{
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

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
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
			this.cancelButton = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.urlText = new System.Windows.Forms.TextBox();
			this.openButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// cancelButton
			// 
			this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.cancelButton.Location = new System.Drawing.Point(456, 64);
			this.cancelButton.Name = "cancelButton";
			this.cancelButton.Size = new System.Drawing.Size(72, 24);
			this.cancelButton.TabIndex = 2;
			this.cancelButton.Text = "Cancel";
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(16, 8);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(512, 24);
			this.label1.TabIndex = 0;
			this.label1.Text = "Enter a URL:";
			// 
			// urlText
			// 
			this.urlText.Location = new System.Drawing.Point(16, 32);
			this.urlText.Name = "urlText";
			this.urlText.Size = new System.Drawing.Size(512, 20);
			this.urlText.TabIndex = 1;
			this.urlText.Text = "";
			// 
			// openButton
			// 
			this.openButton.Location = new System.Drawing.Point(376, 64);
			this.openButton.Name = "openButton";
			this.openButton.Size = new System.Drawing.Size(72, 24);
			this.openButton.TabIndex = 2;
			this.openButton.Text = "Open";
			this.openButton.Click += new System.EventHandler(this.openButton_Click);
			// 
			// OpenDialog
			// 
			this.AcceptButton = this.openButton;
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.CancelButton = this.cancelButton;
			this.ClientSize = new System.Drawing.Size(550, 98);
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
			this.Load += new System.EventHandler(this.OpenDialog_Load);
			this.ResumeLayout(false);

		}
		#endregion

		private void OpenDialog_Load(object sender, System.EventArgs e)
		{

		}

		private void openButton_Click(object sender, System.EventArgs e)
		{
		}
	}
}
