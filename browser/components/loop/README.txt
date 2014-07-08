This is the directory for the Loop desktop implementation and the standalone
client.

The desktop implementation is the UX built into Firefox, activated by the Loop
button on the toolbar. The standalone client is the link-clicker UX for any
modern browser that supports WebRTC.

The standalone client is a set of web pages intended to be hosted on a
standalone server referenced by the loop-server.

The standalone client exists in standalone/ but shares items
(from content/shared/) with the desktop implementation. See the README.md
file in the standalone/ directory for how to run the server locally.


Working with JSX
================

You need to install the JSX compiler using npm in order to compile the .jsx
files into regular .js ones:

    npm install -g react-tools

Once installed, run build-jsx with the --watch option from
browser/components/loop, eg.:

    cd browser/components/loop
    ./build-jsx --watch

build-jsx can also be do a one-time compile pass instead of watching if
the --watch argument is omitted.  Be sure to commit any transpiled files
at the same time as changes to their sources.
