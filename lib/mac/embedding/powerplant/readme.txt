
This project shows how to embed a WebShell inside a PowerPlant application.
It is based on some sample code provided by MetroWerks, that's why you won't
see the usual NPL in any of the source files.


The code is similar to the one originally posted to the newsgroup last May.
The differences are:
- It includes the fixes posted by Douglas L. Mac in news://news.mozilla.org/37CF3A45.D9D25725%40worldnet.att.net
- The output directory is mozilla/dist/viewer_debug
- The precompiled headers contain config/mac/DefinesMac.h and config/mac/DefinesMozilla.h

As of today (September 20th, 1999), it runs but...
- Text selection doesn't work in the HTML area. Maybe other things
  have been broken too in the past few months.
- The menubar is a bit corrupted by the SIOUX window.
- In order to load the default page (test2.html), the 'Samples' folder
  has to be stored at the same level as the application instead of
  inside the 'res' folder. Maybe this has been fixed in the past few days,
  my tree is a bit old.


If you wish to contribute, please contact Mike Pinkerton <pinkerton@netscape.com>
or Pierre Saslawsky <pierre@netscape.com>.