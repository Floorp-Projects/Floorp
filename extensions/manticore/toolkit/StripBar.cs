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

namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.Collections;
  using System.ComponentModel;
  using System.Drawing;
  using System.Drawing.Drawing2D;
  using System.Windows.Forms;


  /// <summary>
	/// Summary description for StripBar.
	/// </summary>
	public class StripBar : UserControl
	{
		public StripBar()
		{
		}

    public ArrayList Rows = new ArrayList();
    public ArrayList Bands = new ArrayList();

    ///////////////////////////////////////////////////////////////////////////
    // IStripBar Implementation
    public void AddBand (StripBand aBand)
    {
      Bands.Add(aBand);
      aBand.Bar = this;

      if (aBand.NewRow) 
      {
        StripRow row = new StripRow(this);
        Rows.Add(row);
        aBand.Row = row;

        row.Bands.Add(aBand);

        // TODO: Trigger Height-Changed Event
      }
      else 
      {
        StripRow row;
        if (Rows.Count >= 1) 
          row = Rows[Rows.Count-1] as StripRow;
        else 
        {
          row = new StripRow(this);
          Rows.Add(row);
        }
        row.Bands.Add(aBand);
        aBand.Row = row;

        // Invalidate Row
        Invalidate(row.Bounds);
      }
    }

    public void RemoveBand(StripBand aStripBand)
    {

    }

    ///////////////////////////////////////////////////////////////////////////
    // Overriden Methods
    protected override void OnPaint(PaintEventArgs aPea)
    {
      int rowCount = Rows.Count;
      for (int i = 0; i < rowCount; ++i) 
      {
        StripRow currRow = Rows[i] as StripRow;
        if (currRow.Bounds.IntersectsWith(aPea.ClipRectangle)) 
          currRow.PaintRow(aPea);

      }
    }
  }

  public class StripBand
  {
    public StripBand()
    {
    }

    ///////////////////////////////////////////////////////////////////////////
    // IStripBand Implementation
    protected StripRow mRow;
    public StripRow Row
    {
      get 
      {
        return mRow;
      }
      set 
      {
        if (value != mRow)
          mRow = value;
      }
    }


    protected StripBar mBar;
    public StripBar Bar
    {
      get 
      {
        return mBar;
      }
      set 
      {
        if (value != mBar) 
          mBar = value;
      }
    }


    protected int mWidth;
    public int Width
    {
      get 
      {
        return mWidth;
      }
      set 
      {
        if (value != mWidth) 
          mWidth = value;
      }
    }


    protected int mHeight = 24;
    public int Height
    {
      get 
      {
        // Compute height:
        // height = the larger of - decoration area (icon/text)
        //                        - client area (control)
        //          + nonclient, nondecoration area (borders)
        //
        return mHeight;
      }
    }


    protected bool mNewRow = false;
    public bool NewRow 
    {
      get 
      {
        return mNewRow;
      }
      set 
      {
        if (value != mNewRow) 
        {
          mNewRow = value;
        }
      }
    }


    protected Control mControl = null;
    public Control Child 
    {
      get 
      {
        return mControl;
      }
      set 
      {
        if (value != mControl) 
        {
          mControl = value;
        }
      }
    }


    protected Color mBackColor = Color.Red;
    public Color BackColor 
    {
      get 
      {
        return mBackColor;
      }
      set 
      {
        if (value != mBackColor) 
        {
          mBackColor = value;
          StripBar bar = mBar as StripBar;
          bar.Invalidate(Bounds);
        }
      }
    }

    public Rectangle Bounds
    {
      get 
      {
        int x, y, w, h, i;

        int bandCount = mRow.Bands.Count;
        for (i = 0, x = 0; i < bandCount; ++i) 
        {
          StripBand currBand = mRow.Bands[i] as StripBand;
          x += currBand.Bounds.Width;
        }
        y = mRow.Bounds.Y;

        h = Height;
        
        w = mBar.Width;
        int bandIndex = mRow.Bands.IndexOf(this);
        if (bandIndex == (mRow.Bands.Count - 1)) 
          w = mBar.ClientRectangle.Width - x;

        return new Rectangle(x, y, w, h);
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    // 
    public void PaintBand(PaintEventArgs aPea)
    {
      SolidBrush sbr = new SolidBrush(BackColor);
      aPea.Graphics.FillRectangle(sbr, Bounds);
    }
  }

  public class StripRow
  {
    public StripRow(StripBar aStripBar)
    {
      mStripBar = aStripBar;
    }

    protected StripBar mStripBar;

    public ArrayList Bands = new ArrayList();

    public Rectangle Bounds
    {
      get 
      {
        int x, y, w, h;
        w = mStripBar.ClientRectangle.Width;
        h = Height;

        x = mStripBar.ClientRectangle.Left;

        int rowCount = mStripBar.Rows.Count;
        StripRow currRow = mStripBar.Rows[0] as StripRow; 
        int i = 0;
        for (y = 0; currRow != this; ++i) 
          y += currRow.Height;

        return new Rectangle(x, y, w, h);
      }
    }

    protected int mHeight = 24;
    public int Height 
    {
      get 
      {
        return mHeight;
      }
      set 
      {
        if (value != mHeight) 
          mHeight = value;
      }
    }

    public void AddBand(StripBand aBand)
    {
      if (aBand.Height > mHeight)
        mHeight = aBand.Height;
        
      Bands.Add(aBand);
    }

    public void PaintRow(PaintEventArgs aPea)
    {
      int bandCount = Bands.Count;
      for (int i = 0; i < bandCount; ++i) 
      {
        StripBand currBand = Bands[i] as StripBand;
        currBand.PaintBand(aPea);
      }
    }
  }
}
