/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

package com.netscape.jsdebugging.ifcui.palomar.widget;

import netscape.application.*;
import netscape.util.*;
import java.awt.image.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

/**
 * A special button that pops up when the mouse is moved over it.
 */

public class PopupButton extends ShapeableView
{

    /**
     * Create a tool with a target and images
     * @param name The name of the button
     * @param note the note that appears when the mouse moves over the button
     * @param target the target to be called when this tool is clicked
     * @param command the string command to be called when this tool is clicked
     * @param image the image that appears when the mouse is not over the button
     * @param altImage the image that appers when the mouse is over the button
     */
    public PopupButton(String label, Target clickTarget, String command, Bitmap image, Bitmap altImage)
    {
        // set the targets
        _target = clickTarget;
        _command = command;
        if (image != null && altImage == null)
            altImage = getHighlitedImageFor(image);

        if (_raisedBorder == null)
           _raisedBorder = BezelBorder.raisedBezel();

        if (_loweredBorder == null)
           _loweredBorder = BezelBorder.loweredBezel();

        setLayoutManager(new MarginLayout());

        _borderView = new BorderView(_raisedBorder);
        _borderView.setHideBorder(true);

        _vbox = new BoxView(BoxView.VERT);

        _imageView = new ImageView();
        _labelView = new StringView();

        _vbox.addFixedView(_imageView);
        _vbox.addSpring(0);
        _vbox.addFixedView(_labelView);
        _bottomSpring = _vbox.addStrut(0);
        _borderView.addSubview(_vbox);
        addSubview(_borderView);

        setImage(image);
        setAlternateImage(altImage);
        setLabel(label);
    }

    public void setImage(Bitmap image)
    {
        _image = image;
        _imageView.setImage(image);
        if (_image == null)
            _bottomSpring.setSpringConstant(1);
         else
            _bottomSpring.setSpringConstant(0);

    }

    public void setAlternateImage(Bitmap image)
    {
        _altImage = image;
    }

    public void showLabel(boolean show)
    {
        if (_showLabel != show)
        {
            _showLabel = show;
            if (_showLabel)
                _labelView.setLabel(_label);
            else
                _labelView.setLabel(null);

            _vbox.resize();
        }
    }

    public void setLabel(String label)
    {
        _label = label;
        if (_showLabel) {
           _labelView.setLabel(label);
           _vbox.resize();
        }
    }

    public void setCommand(String command)
    {
       _command = command;
    }

    public void setTarget(Target target)
    {
       _target = target;
    }

    public boolean mouseDown(MouseEvent event)
    {
       if (_enabled == false)
          return true;

       _pressed = true;
       _inside = true;

       _borderView.setBorder(_loweredBorder);

       draw();
       return true;
    }

    public void mouseUp(MouseEvent event)
    {
       _pressed = false;

       Rect r = bounds();
       boolean mouseInside = r.contains(r.x + event.x, r.y + event.y);

       if (mouseInside && _enabled)
       {
          if( _canToggle )
             _checked = ! _checked;
          if (_target != null)
             _target.performCommand(_command, this);
       }

       if( _canToggle &&  _checked )
          _borderView.setBorder(_loweredBorder);
       else
          _borderView.setBorder(_raisedBorder);

       draw();
      _inside = false;
    }

    public void mouseDragged(MouseEvent event)
    {
       Rect r = bounds();

       boolean mouseInside = r.contains(r.x + event.x, r.y + event.y);

       if (_pressed == true)
          if (_inside == true && mouseInside == false)
          {
             _inside = false;
            // draw();
          } else if (_inside == false && mouseInside == true) {
             _inside = true;
            // draw();
          }
    }

    public void mouseEntered(MouseEvent event)
    {
       // if we are enabled then show our alternate image
       if (_enabled == true)
       {
      //    System.out.println("Entered");
          _highlighted = true;
          _borderView.setHideBorder(false);

          _borderView.layoutView(0,0);

          if (_altImage != null)
             _imageView.setImage(_altImage);

          draw();
       }
    }

    public void mouseExited(MouseEvent event)
    {
      // if we are enabled then show our alternate image
       if (_enabled == true)
       {
         //  System.out.println("Exited");

          _highlighted = false;
          _imageView.setImage(_image);
       }

       // hide border even if we are disabled (but not if checked)
       if( ! _canToggle ||  ! _checked )
          _borderView.setHideBorder(true);

       draw();
    }

    public View viewForMouse(int x, int y)
    {
        if (containsPoint(x,y))
            return this;

         return null;
    }

    public String getLabel()
    {
        return _label;
    }

    public void setEnabled(boolean enabled)
    {
       _enabled = enabled;

       if (_enabled==false) {
          if (_disabledImage == null && _image != null)
             _disabledImage = getDisabledImageFor(_image);

          _imageView.setImage(_disabledImage);
          _labelView.setColor(Color.gray);

       } else {
          _imageView.setImage(_image);
          _labelView.setColor(Color.black);
       }

       _imageView.setDirty(true);
       _labelView.setDirty(true);
       setDirty(true);
    }

    public boolean isEnabled()
    {
       return _enabled;
    }


    public void setChecked(boolean checked)
    {
       if( ! _canToggle )
          return;
       if( _checked == checked )
          return;

       _checked = checked;

       if (_checked==false) {
          _borderView.setBorder(_raisedBorder);
          _borderView.setHideBorder(true);
       } else {
          _borderView.setBorder(_loweredBorder);
          _borderView.setHideBorder(false);
       }

       _imageView.setDirty(true);
       _labelView.setDirty(true);
       setDirty(true);
    }

    public boolean isChecked()
    {
       return _checked;
    }

    public boolean canToggle()
    {
       return _canToggle;
    }

    public void setCanToggle(boolean b)
    {
       _canToggle = b;
    }

    public void drawView(Graphics g)
    {
       if (isTransparent() == false) {
          g.setColor(Color.lightGray);
          g.fillRect(0,0,width(),height());
       }

       super.drawView(g);
    }

    /**
     * Given a bitmap returns a grayscale equivalent.
     */
    public static Bitmap getDisabledImageFor(Bitmap image)
    {
        // create a grayscale image filter
        GrayScaleImageFilter filter = new GrayScaleImageFilter();

        // get the image's producer
        ImageProducer producer = AWTCompatibility.awtImageProducerForBitmap(image);

        // create a filtered source that filters the producer through the grayscale image filter
        FilteredImageSource source = new FilteredImageSource(producer, filter);

        // create a bitmap that uses our new filtered producer
        return AWTCompatibility.bitmapForAWTImageProducer(source);
    }

    /**
     * Given a bitmap returns a grayscale equivalent.
     */
    public static Bitmap getHighlitedImageFor(Bitmap image)
    {
        // create a grayscale image filter
        HighlightingImageFilter filter = new HighlightingImageFilter();

        // get the image's producer
        ImageProducer producer = AWTCompatibility.awtImageProducerForBitmap(image);

        // create a filtered source that filters the producer through the grayscale image filter
        FilteredImageSource source = new FilteredImageSource(producer, filter);

        // create a bitmap that uses our new filtered producer
        return AWTCompatibility.bitmapForAWTImageProducer(source);
    }


    private ImageView  _imageView  = null;
    private StringView  _labelView  = null;
    private Bitmap  _altImage      = null;
    private Bitmap  _image         = null;
    private Bitmap  _disabledImage = null;
    private boolean _highlighted   = false;
    private boolean _pressed       = false;
    private boolean _enabled       = true;
    private String  _label         = "";
    private Target  _target        = null;
    private String  _command       = null;
    private boolean _inside        = false;
    private BorderView _borderView;
    private BoxView _vbox;
    private MarginLayout _margin;
    private Spring _bottomSpring;
    private boolean _showLabel = true;

    private boolean _canToggle  = false;
    private boolean _checked    = false;

    private static Border  _raisedBorder  = null;
    private static Border  _loweredBorder = null;
    private static int _borderPad = 4;
}

