
namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.Runtime.InteropServices;

  public class ThemeStuff 
  {
    [DllImport("uxtheme.dll")]
    public static extern IntPtr OpenThemeData(IntPtr aHWnd, string aClassList);

    [DllImport("uxtheme.dll")]
    public static extern IntPtr CloseThemeData(IntPtr aHTheme);

    public static int TS_NORMAL     = 1;
    public static int TS_HOVER      = 2;
    public static int TS_ACTIVE     = 3;
    public static int TS_DISABLED   = 4;
    public static int TS_FOCUSED    = 5;

    public static int BP_BUTTON     = 1;
    public static int BP_RADIO      = 2;
    public static int BP_CHECKBOX   = 3;
 
    [StructLayout(LayoutKind.Explicit)]
      public struct Rect 
    {
      [FieldOffset(0)]  public int left;
      [FieldOffset(4)]  public int top;
      [FieldOffset(8)]  public int right;
      [FieldOffset(12)] public int bottom;
    }   

    [DllImport("uxtheme.dll")]
    public static extern IntPtr DrawThemeBackground(IntPtr aHTheme, IntPtr aHDC,
      int aPartID, int aStateID,
      ref Rect aRect, ref Rect aClipRect);
  }
}

/*
  IntPtr hTheme = ThemeStuff.OpenThemeData(Handle, "Button");
  IntPtr hDC = e.Graphics.GetHdc();

  ThemeStuff.Rect rect = new ThemeStuff.Rect();
  rect.left = ClientRectangle.Left;
  rect.top = ClientRectangle.Top;
  rect.bottom = ClientRectangle.Bottom;
  rect.right = ClientRectangle.Right;

  Rectangle clipRectangle = e.ClipRectangle;
  ThemeStuff.Rect clipRect = new ThemeStuff.Rect();
  clipRect.left = clipRectangle.Left;
  clipRect.top = clipRectangle.Top;
  clipRect.bottom = clipRectangle.Bottom;
  clipRect.right = clipRectangle.Right;

  ThemeStuff.DrawThemeBackground(hTheme, hDC, ThemeStuff.BP_BUTTON, 
  ThemeStuff.TS_HOVER, ref rect, ref clipRect);

  e.Graphics.ReleaseHdc(hDC);
  ThemeStuff.CloseThemeData(hTheme);
*/