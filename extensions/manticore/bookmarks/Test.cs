using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace Silverstone.Manticore.bookmarks
{
	/// <summary>
	/// Summary description for Test.
	/// </summary>
	public class Test : System.Windows.Forms.Form
	{
    private System.Windows.Forms.StatusBar statusBar1;
    private System.Windows.Forms.TreeView treeView1;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public Test()
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
      this.statusBar1 = new System.Windows.Forms.StatusBar();
      this.treeView1 = new System.Windows.Forms.TreeView();
      this.SuspendLayout();
      // 
      // statusBar1
      // 
      this.statusBar1.Location = new System.Drawing.Point(0, 253);
      this.statusBar1.Name = "statusBar1";
      this.statusBar1.TabIndex = 0;
      this.statusBar1.Text = "statusBar1";
      // 
      // treeView1
      // 
      this.treeView1.Dock = System.Windows.Forms.DockStyle.Fill;
      this.treeView1.ImageIndex = -1;
      this.treeView1.Name = "treeView1";
      this.treeView1.SelectedImageIndex = -1;
      this.treeView1.TabIndex = 1;
      this.treeView1.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.treeView1_AfterSelect);
      // 
      // Test
      // 
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.ClientSize = new System.Drawing.Size(292, 273);
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.treeView1,
                                                                  this.statusBar1});
      this.Name = "Test";
      this.Text = "Test";
      this.ResumeLayout(false);

    }
		#endregion

    private void treeView1_AfterSelect(object sender, System.Windows.Forms.TreeViewEventArgs e)
    {

    }
	}
}
