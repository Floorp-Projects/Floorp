wxEmbed is a sample application developed with wxWindows to demonstrate
various ways of embedding Gecko. It contains a sample browser and several
other demonstrations of embedded Gecko.

wxEmbed is deliberately not based upon wxMozilla - a 3rd party wrapper
for wxWindows. Go with wxMozilla if you are more interested in something more
robust and cross-platform. wxEmbed is intended more as a harness to test the
embedding functionality of Gecko not an end solution.

To build wxEmbed you must:

1. Get the latest 2.4 (at least 2.4.1) release from wxwindows.org
2. Build the debug/release static lib versions
3. cd into wxWindows/control and build src/xrc and utils/wxrc
4. Set your WXWIN environment variable to point to the wxWindows dir
5. Build Mozilla
6. Set MOZ_SRC environment variable to point to it.
7. Call nmake /f makefile.vc

