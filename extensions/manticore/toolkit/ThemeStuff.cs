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