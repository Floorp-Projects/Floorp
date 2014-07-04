This is the directory for the Loop desktop implementation and the standalone client.

The desktop implementation is the UX built into Firefox, activated by the Loop button on the toolbar. The standalone client is the link-clicker UX for any modern browser that supports WebRTC.

The standalone client is a set of web pages intended to be hosted on a standalone server referenced by the loop-server.

The standalone client exists in standalone/ but shares items (from content/shared/) with the desktop implementation. See the README.md file in the standalone/ directory for how to run the server locally.

Working with JSX
================

You need to install the JSX compiler in order to compile the .jsx files into regular .js ones.

The JSX compiler is installable using npm:

    npm install -g react-tools

Once installed, run it with the --watch option, eg.:

    jsx --watch --x jsx browser/components/loop/content/js/src \
                        browser/components/loop/content/js
