Want to generate a CAB file for the plugin host control?

First off you need the Cabinet SDK:

http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dncabsdk/html/cabdl.asp

Install that and edit makecab.bat and specify the path to your cabarc.exe program in the CABARCEXE variable.

Next build the pluginhostctrl.dll (e.g. the ReleaseMinSize target) and type

  makecab ..\ReleaseMinSize\pluginhostctrl.dll

This produces pluginhostctrl.cab which you can place on a webserver somewhere so IE can download it
when it's specified in the <OBJECT> tag.

STILL TO DO

  o  Installation from CAB file has not been tested
  o  Signing the CAB file. I'm still looking for the signing tool. It's
     in a crypto SDK I think.

