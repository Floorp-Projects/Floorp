ActiveX control for hosting Netscape plugins in IE
==================================================

This folder contains an ActiveX control that allows you to host
NP API compatible plugins within Internet Explorer. It's pretty simple to
use and is a self-contained DLL called pluginhostctrl.dll. It has no runtime
or build dependencies on any other
part of the Mozilla


To build
========

1. Open pluginhostctrl.dsp
2. Build "Win32 Debug" or another target
3. Open some of the test files in IE to verify it works

The control currently reads plugins from your most current Netscape 4.x
installation. There is no support for Mozilla or NS 6.x yet.

A note to developers: If you intend to modify this control IN ANY WAY then
you MUST also change the CLSID. This is necessary to prevent your control
from registering itself over this control or vice versa.

Use a tool such as guidgen.exe or uuidgen.exe to create a new CLSID.


Manual installation
===================

Assuming you have the precompiled DLL and wish to install it on a machine,
you must register it:

regsvr32 pluginhostctrl.dll

You must have administrator privileges to install a new control on
operating systems such as Windows NT, 2000 & XP.


Usage
=====

Insert some HTML like this into your content:

<OBJECT CLASSID="CLSID:DBB2DE32-61F1-4F7F-BEB8-A37F5BC24EE2"
     WIDTH="500" HEIGHT="300">
  <PARAM name="type" value="video/quicktime"/>
  <PARAM name="src" value="http://www.foobar.com/some_movie.mov"/>
  <!-- Custom arguments -->
  <PARAM name="loop" value="true"/>
</OBJECT>

The CLSID attribute tells IE to create an instance of the plugin hosting
control, the width and height specify the dimensions in pixels.

The following <EMBED> attributes have <PARAM> tag equivalents:

* <PARAM name="type" ...> is equivalent to TYPE
  Specifies the MIME type of the plugin. The control uses this value to
  determine which plugin it should create to handle the content.

* <PARAM name="src" ...> is equivalent to SRC
  Specifies an URL for the initial stream of data to feed the plugin.
  If you havent't specified a "type" PARAM, the control will attempt to use
  the MIME type of this stream to create the correct plugin.

* <PARAM name="pluginspage" ...> is equivalent to PLUGINSPAGE
  Specifies a URL where the plugin may be installed from. The default
  plugin will redirect you to this page when you do not have the correct
  plugin installed.

You may also supply any custom plugin parameters as additional <PARAM>
elements.


Still to do
===========

* Only hosts windowed plugins.
* Doesn't work for the Adobe Acrobat plugin yet.
* No progress indication to show when there is network activity
* Plugins cannot create writeable streams.
* Package pluginhostctrl.dll into a CAB file automatic installation in IE.
