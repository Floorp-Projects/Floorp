Want to generate a CAB file for the plugin host control?

First off you need the Cabinet SDK:

  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dncabsdk/html/cabdl.asp

And if you want to sign the CAB file you need the signing tool:

  http://msdn.microsoft.com/MSDN-FILES/027/000/219/codesign.exe

More blurb about signing is here:

  http://www.verisign.com/support/tlc/codesign/install.html

Please don't email me questions about signing since I'm just following the
instructions in the above link.

Install the tools somewhere and edit the line setting the PATH in makecab.bat
to specify the correct path(s) to these programs.

Next build the pluginhostctrl.dll (e.g. the ReleaseMinSize target) and type

  makecab ..\ReleaseMinSize\pluginhostctrl.dll

This produces pluginhostctrl.cab which you can place on a webserver somewhere
so IE can download it when it's specified in the <OBJECT> tag, e.g.

  <OBJECT width="100" height="100"
    classid="CLSID:DBB2DE32-61F1-4F7F-BEB8-A37F5BC24EE2"
    codebase="http://www.blahblah.com/pluginhostctrl.cab#Version=1,0,0,0">
  </OBJECT>

If you want to sign the CAB file with a certificate, type

  makecab ..\ReleaseMinSize\pluginhostctrl.dll xx.cer xx.key xx.spc

Where xx.* files are your certificate, key and your software publisher
certificate.

There is a sample verify.htm page where you may try and install the control from.

STILL TO DO

  o  Installation from the CAB will probably fail until these scripts have been
     sorted out properly.