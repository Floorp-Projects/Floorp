using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace Silverstone.Manticore.Toolkit
{
	/// <summary>
	/// Summary description for TestForm.
	/// </summary>
	public class TestForm : System.Windows.Forms.Form
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public TestForm()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      StripBar bar = new StripBar();
//      bar.BackImage = Image.FromFile("resources\\manticore.png");
      this.Controls.Add(bar);
      bar.Location = new Point(0, 0);
      bar.Size = new Size(500, 75);
      

      Band bnd = new Band();
//      bnd.BackColor = SystemColors.ControlDarkDark;
      bar.Bands.Add(bnd);

      bnd = new Band();
//      bnd.BackColor = SystemColors.ControlLight;
      bnd.NewRow = true;
      bar.Bands.Add(bnd);


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
			this.components = new System.ComponentModel.Container();
			this.Size = new System.Drawing.Size(300,300);
			this.Text = "TestForm";
		}
		#endregion
	}
}
