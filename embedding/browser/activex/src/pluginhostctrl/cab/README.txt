Mozilla ActiveX Control for hosting Plug-ins

Adam Lock <adamlock@netscape.com>


Want to generate a CAB file for the plugin host control?

First off you need the Cabinet SDK and the signing tools:

  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dncabsdk/html/cabdl.asp
  http://msdn.microsoft.com/MSDN-FILES/027/000/219/codesign.exe

More informative blurb about signing is here:

  http://www.verisign.com/support/tlc/codesign/install.html

Please don't email me questions about signing since I'm just following the
instructions in these links.

Install the CAB and signing tools somewhere and edit the line in makecab.bat
to specify the correct PATH to these programs.

Next build the pluginhostctrl.dll (e.g. from the ReleaseMinSize target) and type

  makecab ..\ReleaseMinSize\pluginhostctrl.dll

This produces pluginhostctrl.cab which you can place on a webserver somewhere
so IE can download it when it's specified in the <OBJECT> tag, e.g.

  <OBJECT width="100" height="100"
    classid="CLSID:DBB2DE32-61F1-4F7F-BEB8-A37F5BC24EE2"
    codebase="http://www.blahblah.com/pluginhostctrl.cab#Version=1,0,0,1">
  </OBJECT>

The makecab script automatically signs the CAB file with a TEST CERTIFICATE. YOu
will be prompted to create or enter a password - just make one up and remember
it.

   DO NOT DISTRIBUTE CAB FILES SIGNED WITH THE TEST CERTIFICATE!!!!

If you want to sign the CAB file with a real certificate, run makecab like this:

  makecab ..\ReleaseMinSize\pluginhostctrl.dll xx.cer xx.key xx.spc

Where xx.* files are your real certificate, key and your software publisher
certificate. If you don't have a real certificate you need to visit somewhere
like Verisign and buy one.

There is a sample verify.htm page that you may load in IE to see if the
control installs and runs correctly.

STILL TO DO:

  o  Figure out what's wrong with pluginhostctrl.inf that stops atl.dll from 
     installing. For the time being atl.dll is commented out of the .inf file
     which means it won't be installed and the plugin won't work if its not
     installed already.
  o  Script is untested with a real key/cert/spc but should work.
