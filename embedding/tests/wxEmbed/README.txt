wxEmbed is a sample application developed with wxWindows to demonstrate
various ways of embedding Gecko. It contains a sample browser and several
other demonstrations of embedded Gecko.

wxEmbed is deliberately not based upon wxMozilla - a 3rd party wrapper
for wxWindows. wxMozilla is the recommended route if you are more interested
in something more robust and cross-platform for wxWindows. wxEmbed is intended
more as a test harness for the embedding functionality of Gecko not an end
solution.

wxMozilla is here:

http://wxmozilla.sourceforge.net/

To build wxEmbed you must:

1. Get the latest 2.4 (at least 2.4.1) release from wxwindows.org
2. Build the debug/release static lib versions
3. cd into wxWindows/contrib and build src/xrc and utils/wxrc
4. Set your WXWIN environment variable to point to the wxWindows dir
5. Build Mozilla
6. Set MOZ_SRC environment variable to point to it.
7. Call nmake /f makefile.vc

If the gods were kind you should now have a wxembed.exe that you can copy to
your Mozilla dist directory and execute.
