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

    protected override void OnPaint(PaintEventArgs e)
    {
      // Paint Bands
      uint count = Bands.Count;
      for (int i = 0; i < count; ++i) 
      {
        if (e.ClipRectangle.IntersectsWith(Bands[i].Bounds)) 
        {
          Bands[i].PaintBand(e);
        }
      }
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
      // Paint Background
      if (BackImage != null) 
      {
        // Tile the Image
        TextureBrush tbrush = new TextureBrush(BackImage, WrapMode.Tile);
        e.Graphics.FillRectangle(tbrush, Bounds);
      }

      SolidBrush sbr = new SolidBrush(BackColor);
      e.Graphics.FillRectangle(sbr, Bounds);
    }

    protected Color mBackColor = SystemColors.Control;
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
          Invalidate();
        }
      }
    }

    protected Color mForeColor = SystemColors.ControlText;
    public Color ForeColor 
    {
      get 
      {
        return mForeColor;
      }
      set 
      {
        if (value != mForeColor) 
        {
          mForeColor = value;
          Invalidate();
        }
      }
    }

    protected bool mLocked = false;
    public bool Locked
    {
      get 
      {
        return mLocked;
      }
      set 
      {
        if (value != mLocked) 
        {
          mLocked = value;
          // Invalidate to toggle grippers
        }
      }
    }

    
    public enum BarOrientation : uint
    {
      Horizontal, Vertical
    }

    protected BarOrientation mOrientation = BarOrientation.Horizontal;
    public BarOrientation Orientation
    {
      get 
      {
        return mOrientation;
      }
      set 
      {
        if (value != mOrientation) 
        {
          mOrientation = value;
          Invalidate();
        }
      }
    }
    

    protected Image mBackImage = null;
    public Image BackImage 
    {
      get 
      {
        return mBackImage;
      }
      set 
      {
        if (value != mBackImage) 
        {
          mBackImage = value;
          Invalidate();
        }
      }
    }
    

    protected BandCollection mBands = null;
    public BandCollection Bands 
    {
      get 
      {
        if (mBands == null) 
          mBands = new BandCollection(this);
        return mBands;
      }
    }

    public class BandCollection 
    {
      public BandCollection(StripBar aOwner)
      {
        mOwner = aOwner;
      }

      protected StripBar mOwner;

      // protected Hashtable mBands = null;
      protected ArrayList mBandsList = new ArrayList();

      public Band this [int aIndex]
      {
        get 
        {
          return mBandsList[aIndex] as Band;
        }
        set 
        {
          value.Owner = mOwner;
          mBandsList[aIndex] = value;
        }
      }


      public void Add (Band aBand) 
      {
        aBand.Owner = mOwner;

        Band lastBand = null;
        if (mBandsList.Count > 0)
          lastBand = mBandsList[mBandsList.Count-1] as Band;

        int x, y, w, h;

        if (aBand.NewRow) 
        {
          // We're the first band on a new row
          x = mOwner.ClientRectangle.Left;
          if (lastBand != null) 
            y = lastBand.Bounds.Top + lastBand.Bounds.Height + 1;
          else
            y = mOwner.ClientRectangle.Top;
        }
        else 
        {
          // We're (possibly) on the same row as another band
          if (lastBand != null)
            x = lastBand.Bounds.Left + lastBand.Bounds.Width + 1;
          else
            x = mOwner.ClientRectangle.Left;
          if (lastBand != null) 
            y = lastBand.Bounds.Top;
          else
            y = mOwner.ClientRectangle.Top;
        }

        w = mOwner.ClientRectangle.Width - x - 1;
        h = 24; // XXX (2)

        aBand.Bounds = new Rectangle(x, y, w, h);
          
        mBandsList.Add(aBand);

        mOwner.Invalidate(); // XXX (1)
      }

      public void Remove (Band aBand)
      {
        mBandsList.Remove(aBand);
        aBand.Owner = null;
        mOwner.Invalidate(); // XXX (1)
      }

      public void Clear ()
      {
        mBandsList.Clear();
      }

      public uint Count
      {
        get 
        {
          return (uint) mBandsList.Count;
        }
      }
    }
  }

  public class Band
  {
    public Band()
    {
    }

    protected StripBar mOwner;
    public StripBar Owner 
    {
      get 
      {
        return mOwner;
      }
      set 
      {
        if (value != mOwner) 
        {
          mOwner = value;
        }
      }
    }


    protected Rectangle mBounds;
    public Rectangle Bounds 
    {
      get 
      {
        return mBounds;
      }
      set 
      {
        if (value != mBounds) 
        {
          mBounds = value;
          if (mOwner != null)
            mOwner.Invalidate(Bounds);
        }
      }
    }


    protected Color mForeColor = SystemColors.ControlText;
    public Color ForeColor 
    {
      get 
      {
        return mForeColor;
      }
      set 
      {
        if (value != mForeColor) 
        {
          mForeColor = value;
          // XXX improve this to invalidate only the label
          if (mOwner != null)
            mOwner.Invalidate(Bounds);
        }
      }
    }
    protected Color mBackColor = Color.Transparent;
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
          if (mOwner != null)
            mOwner.Invalidate(Bounds);
        }
      }
    }

    
    protected string mText;
    public string Text 
    {
      get 
      {
        return mText;
      }
      set 
      {
        if (value != mText) 
        {
          mText = value;
        }
      }
    }
    

    protected Image mBackImage = null;
    public Image BackImage 
    {
      get 
      {
        return mBackImage;
      }
      set 
      {
        if (value != mBackImage) 
        {
          mBackImage = value;
          if (mOwner != null)
            mOwner.Invalidate(Bounds);
        }
      }
    }
    

    public uint Index 
    {
      get 
      {
        return 0;
      }
    }

    public string Key
    {
      get 
      {
        return "fluffy_the_magic_goat";
      }
    }


    protected bool mNewRow = true;
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


    protected bool mVisible = true;
    public bool Visible
    {
      get
      {
        return mVisible;
      }
      set 
      {
        if (value != mVisible)
        {
          mVisible = value;
        }
      }
    }

    public void PaintBand(PaintEventArgs e)
    {
      Graphics g = e.Graphics;
      SolidBrush br = new SolidBrush(mBackColor);
      g.FillRectangle(br, Bounds);

      g.DrawLine(SystemPens.ControlLight, Bounds.Left, Bounds.Top, 
        Bounds.Left + Bounds.Width, Bounds.Top);
      g.DrawLine(SystemPens.ControlLight, Bounds.Left, Bounds.Top, 
        Bounds.Left, Bounds.Top + Bounds.Height);
      g.DrawLine(SystemPens.ControlDark, Bounds.Left + Bounds.Width, 
        Bounds.Top, Bounds.Left + Bounds.Width, Bounds.Top + Bounds.Height);
      g.DrawLine(SystemPens.ControlDark, Bounds.Left, Bounds.Top + Bounds.Height,
        Bounds.Left + Bounds.Width, Bounds.Top + Bounds.Height);
    }
  }
}
