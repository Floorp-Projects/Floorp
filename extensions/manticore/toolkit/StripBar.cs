
namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.Collections;
  using System.ComponentModel;
  using System.Drawing;
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
      uint count = Bands.Count;
      for (int i = 0; i < count; ++i) 
      {
        if (e.ClipRectangle.IntersectsWith(Bands[i].Bounds)) 
        {
          Bands[i].PaintBand(e);
        }
      }
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
        mBandsList.Add(aBand);
      }

      public void Remove (Band aBand)
      {
        mBandsList.Remove(aBand);
        aBand.Owner = null;
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
    }
  }
}
